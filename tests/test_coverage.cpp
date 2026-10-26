#include "../src/engineering/CoverageCore.h"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <random>

#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL line "<<__LINE__<<": "<<#x<<'\n';std::exit(1);}}while(false)
using namespace lah::engineering;
template<class Fn>void rejects(Fn fn){bool rejected=false;try{fn();}catch(const std::invalid_argument&){rejected=true;}CHECK(rejected);}
int main(){
    CHECK(insideNominalBeam({0,0,0},{0,10},60));
    CHECK(!insideNominalBeam({0,0,0},{0,-10},60));
    CHECK(!insideNominalBeam({0,0,0},{0,0},60));
    CHECK(insideNominalBeam({0,0,0},{5,5*std::sqrt(3.0)},60));
    CHECK(!insideNominalBeam({0,0,0},{5.001,5*std::sqrt(3.0)},60));
    CHECK(insideNominalBeam({0,0,359},{0,10},5));
    CHECK(insideNominalBeam({0,0,-180},{0,-10},5));
    CHECK(insideNominalBeam({0,0,90},{10,0},5));
    CHECK(!insideNominalBeam({0,0,90},{-10,0},5));
    // Independent vector-dot test: a direction is in a cone iff its
    // projection onto the unit forward vector exceeds cos(half beam).
    std::mt19937 rng(7204);std::uniform_real_distribution<double> pos(-100,100),angle(-720,720),beam(1,180);
    for(int index=0;index<5000;++index){
        const FillSource s{pos(rng),pos(rng),angle(rng)};const AudiencePoint p{pos(rng),pos(rng)};const double b=beam(rng);
        const double dx=p.x-s.x,dy=p.y-s.y,heading=s.headingDeg*pi/180;
        const double cosine=(dx*std::sin(heading)+dy*std::cos(heading))/std::hypot(dx,dy);
        CHECK(insideNominalBeam(s,p,b)==(cosine>=std::cos(b*pi/360)));
    }
    const auto ring=circularFill(4,5);
    CHECK(ring.size()==4);CHECK(std::abs(ring[0].y-5)<1e-12);CHECK(std::abs(ring[1].x-5)<1e-12);
    CHECK(std::abs(ring[2].y+5)<1e-12);CHECK(std::abs(ring[3].x+5)<1e-12);
    // On-axis radial listeners must each be in their respective beam.
    for(const auto& s:ring)CHECK(insideNominalBeam(s,{3*s.x,3*s.y},60));
    const auto audience=audienceRing(15,720);const auto coverage=analyseCoverage(ring,audience,90);
    CHECK(coverage.coveredFraction>0 && coverage.coveredFraction<1); // displaced sources leave angular gaps
    const auto coincident=analyseCoverage({{0,0,0},{0,0,90},{0,0,180},{0,0,270}},audience,90);
    CHECK(coincident.coveredFraction==1);CHECK(coincident.overlapFraction<.01);
    const auto fan=circularFill(3,5,90);
    CHECK(std::abs(fan.front().headingDeg+45)<1e-12);CHECK(std::abs(fan.back().headingDeg-45)<1e-12);
    CHECK(std::abs(fan[1].x)<1e-12 && std::abs(fan[1].y)<1e-12);
    CHECK(fan.front().y<0);CHECK(fan.front().x<0);
    const auto rotated=circularFill(1,5,90,180);CHECK(std::abs(rotated[0].headingDeg-180)<1e-12);
    const auto line=audienceLine(20,10,3);CHECK(line.size()==3 && line[0].x==-10 && line[2].x==10);
    const auto text=fillSourcesCsv(ring);const auto decoded=parseFillSourcesCsv(text);
    CHECK(decoded.size()==ring.size());
    for(std::size_t k=0;k<ring.size();++k){CHECK(decoded[k].x==ring[k].x);CHECK(decoded[k].y==ring[k].y);CHECK(decoded[k].headingDeg==ring[k].headingDeg);}
    CHECK(parseFillSourcesCsv(" \r\n1, 2, -30\r\n").size()==1);
    rejects([]{(void)parseFillSourcesCsv("1,2,3,4");});rejects([]{(void)parseFillSourcesCsv("1;2;3");});
    rejects([]{(void)parseFillSourcesCsv("nan,0,0");});rejects([]{(void)parseFillSourcesCsv("1001,0,0");});
    rejects([]{(void)parseFillSourcesCsv("x_m,y_m,heading_deg\n");});
    rejects([]{(void)circularFill(0,5);});rejects([]{(void)circularFill(81,5);});rejects([]{(void)circularFill(2,0);});
    rejects([]{(void)audienceRing(1,2);});rejects([]{(void)audienceLine(20,0,1);});
    rejects([]{(void)insideNominalBeam({0,0,0},{0,10},0);});
    rejects([]{(void)insideNominalBeam({0,0,0},{std::numeric_limits<double>::infinity(),0},60);});
    rejects([]{(void)beamIntersectionAtAudience(0,0,-1,0);});
    const auto many=calculateFill(200,1,140,.75,80,20);CHECK(many.coveringSources.size()==501);
    CHECK(calculateFill(200,1,10,0,1).maxUncoveredGapM<=200);
    std::cout<<"PASS coverage: 5000 independent dot-product comparisons, circle/fan/line geometry, CSV round-trip, bounds\n";
}
