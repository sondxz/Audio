#include "../src/simulation/MeasuredSpeakerCore.h"
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAILED "<<__LINE__<<": "<<#x<<"\n";std::exit(1);}}while(false)
int main(){
    using namespace lah::simulation;
    std::vector<PolarPoint> data;
    for(int angle:{0,90,180,270})for(int f:{80,100,120})
        data.push_back({static_cast<double>(f),static_cast<double>(angle),
                        angle==180?-10.0:0.0,0.0});
    auto profile=PolarProfile::load(data);
    CHECK(profile.valid);CHECK(!profileResponse(profile,70,0).valid);
    auto on=profileResponse(profile,100,0);
    CHECK(on.valid);CHECK(std::abs(std::abs(on.h)-1)<1e-10);
    auto back=profileResponse(profile,100,180);
    CHECK(back.valid);CHECK(std::abs(std::abs(back.h)-std::pow(10.0,-10.0/20))<1e-10);
    auto mid=profileResponse(profile,100,135);
    CHECK(mid.valid);CHECK(std::abs(std::abs(mid.h)-std::pow(10.0,-5.0/20))<1e-10);
    std::vector<Sub> sources={{{0,0,0},0,0,false,true}};
    auto pressure=measuredPressure(sources,{0,1,0},{343,100},profile);
    CHECK(pressure.valid);CHECK(std::abs(std::abs(pressure.h)-1.0)<1e-10);
    sources.push_back(sources[0]);sources[1].invert=true;
    const auto cancelled=measuredPressure(sources,{0,1,0},{343,100},profile);
    CHECK(cancelled.valid && std::abs(cancelled.h)<1e-12);
    CHECK(!profileResponse(profile,std::numeric_limits<double>::quiet_NaN(),0).valid);
    CHECK(!measuredPressure(sources,{0,1,0},{0,100},profile).valid);
    CHECK(!measuredPressure(sources,{0,1,0},{std::numeric_limits<double>::quiet_NaN(),100},profile).valid);
    auto map=simulateMeasured2D(sources,{343,100},16,12,10,15,profile);
    CHECK(map.db.size()==16*12);
    CHECK(simulateMeasured2D(sources,{343,250},16,12,10,15,profile).db.empty());
    data[5].frequencyHz=81;CHECK(!PolarProfile::load(data).valid);
    std::cout<<"PASS: polar dataset angular/frequency gates, -10dB rear, -5dB interpolation, complex pressure/grid\n";
}
