#include "../src/simulation/SubArrayCore.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

#define REQUIRE(x) do{if(!(x)){std::cerr<<"FAILED "<<__LINE__<<": "<<#x<<'\n';std::exit(1);}}while(false)
bool near(double a,double b,double tolerance=1e-10){return std::abs(a-b)<tolerance;}
int main() {
    using namespace lah::simulation;
    const std::vector<Sub> single{{{1,-2,0.5},20*std::log10(0.5),2.5,true,true}};
    const Point3 probe{-1,3,1.5};
    const double r=std::sqrt(30.0),phase=-2*pi*83*(r/343+0.0025);
    const std::complex<double> expected{-0.5*std::cos(phase)/r,-0.5*std::sin(phase)/r};
    REQUIRE(std::abs(pressure(single,probe,{343,83})-expected)<1e-12);
    std::vector<Sub> pair{{{0,0,0},0,0,false,true},{{0,0,0},0,0,false,true}};
    REQUIRE(near(relativeDb(pair,{0,1,0},{343,80}),20*std::log10(2.0)));
    pair[1].invert=true;
    const auto null=responsePoint(pressure(pair,{0,1,0},{343,80}),80);
    REQUIRE(null.valid&&!null.phaseValid&&null.relativeDb==-100);
    REQUIRE(near(std::abs(pressure({pair[0]},{0,0,0},{343,80})),4)); // declared 0.25m singularity floor

    constexpr double c=343,spacing=1,quarterHz=c/(4*spacing),far=10000;
    for(int id=0;id<=6;++id) {
        auto sources=preset(id,spacing,c,1.5,12);
        REQUIRE(!sources.empty()&&validScene(sources,{0,12,1.2},{c,80}));
        for(const auto& source:sources)REQUIRE(source.delayMs>=0&&source.delayMs<=500);
    }
    for(int id:{1,2,4}) {
        const auto sources=preset(id,spacing,c);
        const auto front=std::abs(pressure(sources,{0,far,0.9},{c,quarterHz}));
        const auto rear=std::abs(pressure(sources,{0,-far,0.9},{c,quarterHz}));
        REQUIRE(20*std::log10(front/rear)>65); // asymptotic directivity, no relative-plot floor
    }
    const auto cardioid=preset(1,spacing,c);
    REQUIRE(cardioid.size()==2&&cardioid[1].invert&&near(cardioid[1].delayMs,1000/c));
    const auto endfire=preset(2,spacing,c);
    REQUIRE(endfire.size()==4&&near(endfire[0].delayMs,3000/c)&&endfire[3].delayMs==0);
    REQUIRE(endfire[0].position.y==0&&endfire[3].position.y==-3);
    const auto stack=preset(4,spacing,c);
    REQUIRE(stack.size()==3&&stack[2].invert&&near(stack[2].gainDb,20*std::log10(2.0)));
    REQUIRE(stack[0].position.z<stack[1].position.z&&stack[2].position.y==-spacing);
    const auto arc=preset(5,spacing,c,2,12);
    REQUIRE(near(arc.front().position.y,-2)&&near(arc.back().position.y,-2));
    REQUIRE(near(arc[1].position.y,-2.0/9)&&arc[0].delayMs==0&&arc[1].delayMs==0);
    const auto focused=preset(6,spacing,c,2,12);
    REQUIRE(focused[0].position.y==0&&focused[1].delayMs>focused[0].delayMs);
    const Point3 focus{0,12,0.4};
    double total=0,arrival=-1;
    for(const auto& source:focused) {
        const double current=distance(source.position,focus)/c+source.delayMs*0.001;
        if(arrival>=0)REQUIRE(near(current,arrival));
        arrival=current;total+=1/distance(source.position,focus);
    }
    for(double hz:{20.0,80.0,200.0})REQUIRE(near(std::abs(pressure(focused,focus,{c,hz})),total));
    auto preDelayed=preset(3,2,c);preDelayed[0].delayMs=10;preDelayed[1].invert=true;
    const auto aligned=alignAtPoint(preDelayed,{3,10,1},c);
    REQUIRE(aligned.size()==2&&aligned[0].delayMs>=10&&aligned[1].invert);
    REQUIRE(near(distance(aligned[0].position,{3,10,1})/c+aligned[0].delayMs*0.001,
                 distance(aligned[1].position,{3,10,1})/c+aligned[1].delayMs*0.001));

    const std::vector<Sub> delayed{{{0,0,0},0,2,false,true}};
    const auto arrivalInfo=earliestArrival(delayed,{0,3.43,0},c);
    REQUIRE(arrivalInfo.valid&&near(arrivalInfo.milliseconds,12));
    const auto unaligned=responsePoint(pressure(delayed,{0,3.43,0},{c,100}),100);
    const auto referenced=responsePoint(pressure(delayed,{0,3.43,0},{c,100}),100,12);
    REQUIRE(unaligned.phaseValid&&near(unaligned.phaseDeg,-72,1e-8));
    REQUIRE(referenced.phaseValid&&near(referenced.phaseDeg,0,1e-8));
    const auto trace=frequencyResponse(delayed,{0,3.43,0},c,20,300,160,12);
    REQUIRE(trace.valid&&trace.points.size()==160&&near(trace.points.front().hz,20)&&near(trace.points.back().hz,300));
    for(const auto& point:trace.points)REQUIRE(point.valid&&point.phaseValid&&near(point.phaseDeg,0,1e-8));
    const auto grid=simulate2D(arc,{c,80},32,24,32,44);
    REQUIRE(grid.db.size()==32*24);
    for(const auto db:grid.db)REQUIRE(std::isfinite(db));

    const auto nan=std::numeric_limits<double>::quiet_NaN();
    REQUIRE(preset(99,1).empty()&&preset(1,nan).empty()&&preset(1,1,0).empty());
    REQUIRE(!frequencyResponse(delayed,{nan,0,0},c).valid);
    REQUIRE(!frequencyResponse(delayed,{0,1,0},c,20,300,1).valid);
    REQUIRE(!frequencyResponse(delayed,{0,1,0},c,20,300,160,1e9).valid);
    REQUIRE(!earliestArrival(delayed,{0,1,0},nan).valid);
    REQUIRE(simulate2D(delayed,{c,nan},32,24,32,44).db.empty());
    REQUIRE(simulate2D(delayed,{c,80},32,24,nan,44).db.empty());
    auto invalid=delayed;invalid[0].gainDb=nan;
    REQUIRE(!validScene(invalid,{0,1,0},{c,80}));
    REQUIRE(!std::isfinite(relativeDb(invalid,{0,1,0},{c,80})));
    invalid[0].enabled=false;REQUIRE(validScene(invalid,{0,1,0},{c,80}));
    std::cout<<"PASS: analytic complex sources/cancellation, cardioid/end-fire/stack far field, arc/focus geometry, phase/reference/probes, finite guards\n";
}
