#include "../src/dsp/BuiltinRackCore.h"
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

#define REQUIRE(x) do {if(!(x)){std::cerr<<"FAILED line "<<__LINE__<<": " #x "\n";std::exit(1);}} while(false)

using namespace lah::builtin;
namespace {
// Warm up the filters and measure a coherent one-second tone, excluding transitions.
double toneRms(Rack& rack,Controls& controls,int type,const std::array<float,6>& parameters,double hz,float amplitude=.02f) {
    configureSlot(controls.slots[0][0],type,true);
    for(std::size_t i=0;i<parameters.size();++i)controls.slots[0][0].params[i].store(parameters[i]);
    rack.reset();rack.beginBlock(controls);
    double sum=0;
    for(int i=0;i<72000;++i){
        const float input=amplitude*static_cast<float>(std::sin(2*pi*hz*i/48000));
        const float result=rack.processSample(0,input);
        REQUIRE(std::isfinite(result));
        REQUIRE(rack.processSample(7,input)==input);
        if(i>=24000)sum+=static_cast<double>(result)*result;
    }
    return std::sqrt(sum/48000);
}
std::array<float,6> defaults(int type){
    std::array<float,6> result{};
    for(int i=0;i<6;++i)result[static_cast<std::size_t>(i)]=metadata(type).params[static_cast<std::size_t>(i)].defaultValue;
    return result;
}
}

int main(){
    Controls controls;
    Rack rack;rack.prepare(48000);
    static_assert(moduleCount==17,"Update this suite when extending the console catalog");
    REQUIRE(metadata(Analog).parameterCount==6 && metadata(FourKLine).parameterCount==6);

    // The Analog wet control gives an exact dry path and calibrated output trim.
    auto analog=defaults(Analog);analog[4]=0;analog[5]=6;
    const auto dry=toneRms(rack,controls,Analog,analog,1000);
    REQUIRE(std::abs(dry/(.02/std::sqrt(2.0))-gain(6))<1e-5);

    // Its two tone controls change distinct bands and drive compresses high levels.
    analog={20,0,0,0,100,0};
    const auto analogFlat=toneRms(rack,controls,Analog,analog,150);
    analog[1]=6;
    REQUIRE(std::abs(20*std::log10(toneRms(rack,controls,Analog,analog,150)/analogFlat)-6)<.015);
    analog={20,0,0,0,100,0};
    const auto clean=toneRms(rack,controls,Analog,analog,1000,.5f);
    analog[3]=18;
    REQUIRE(toneRms(rack,controls,Analog,analog,1000,.5f)<clean*.5);

    // Every 4K Line EQ band reaches its specified gain at its center frequency.
    const std::array<double,4> centerHz{100,400,3000,10000};
    for(int band=0;band<4;++band){
        std::array<float,6> line{20,0,0,0,0,0};
        const auto baseline=toneRms(rack,controls,FourKLine,line,centerHz[static_cast<std::size_t>(band)]);
        line[static_cast<std::size_t>(band+1)]=6;
        const auto boosted=toneRms(rack,controls,FourKLine,line,centerHz[static_cast<std::size_t>(band)]);
        REQUIRE(std::abs(20*std::log10(boosted/baseline)-6)<.015);
        line[static_cast<std::size_t>(band+1)]=-9;
        const auto cut=toneRms(rack,controls,FourKLine,line,centerHz[static_cast<std::size_t>(band)]);
        REQUIRE(std::abs(20*std::log10(cut/baseline)+9)<.015);
    }

    // High-pass removes subsonic input and the line drive is a real nonlinearity.
    std::array<float,6> line{20,0,0,0,0,0};
    const auto low=toneRms(rack,controls,FourKLine,line,30);
    line[0]=300;
    REQUIRE(toneRms(rack,controls,FourKLine,line,30)<low*.02);
    line={20,0,0,0,0,0};const auto cleanLine=toneRms(rack,controls,FourKLine,line,1000,.5f);
    line[5]=18;REQUIRE(toneRms(rack,controls,FourKLine,line,1000,.5f)<cleanLine*.5);

    // Module state is channel-local and reset invalidates prior filter history.
    for(int type:{Analog,FourKLine}){
        configureSlot(controls.slots[0][0],type,true);rack.reset();rack.beginBlock(controls);
        for(int i=0;i<10000;++i)(void)rack.processSample(0,.3f);
        rack.resetChannel(0);
        for(int i=0;i<10000;++i)REQUIRE(rack.processSample(0,0)==0);
        for(auto& parameter:controls.slots[0][0].params)parameter.store(std::numeric_limits<float>::quiet_NaN());
        rack.beginBlock(controls);REQUIRE(std::isfinite(rack.processSample(0,.2f)));
    }
    std::cout<<"PASS: Analog dry/output law, EQ/drive; four 4K Line band gains, HPF, drive, channel isolation and reset\n";
}
