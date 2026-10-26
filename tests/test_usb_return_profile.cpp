#include "../src/audio/UsbReturnProfile.h"
#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

#define REQUIRE(condition) do { if (!(condition)) { std::cerr << "FAILED line " << __LINE__ << ": " #condition "\n"; std::exit(1); } } while(false)

namespace {
using ChannelControls=std::array<std::atomic<float>,lah::dsp::channels>;
std::array<ChannelControls*,15> retainedDspFields(lah::dsp::Controls& controls){
    return {&controls.inputGain,&controls.eqDb,&controls.eqHz,&controls.eqQ,&controls.hpfHz,
        &controls.compThreshold,&controls.compRatio,&controls.compAttackMs,&controls.compReleaseMs,
        &controls.gateThreshold,&controls.outputGain,&controls.outputDelayMs,&controls.outputPolarity,
        &controls.outputCeiling,&controls.inputMute};
}

void seedControls(lah::dsp::Controls& controls){
    int fieldIndex=1;
    for(auto* field:retainedDspFields(controls)){
        for(int channel=0;channel<lah::dsp::channels;++channel)
            (*field)[static_cast<std::size_t>(channel)].store(static_cast<float>(fieldIndex*32+channel)*.01f);
        ++fieldIndex;
    }
    for(int channel=0;channel<lah::dsp::channels;++channel){
        controls.inputPan[static_cast<std::size_t>(channel)].store(channel%2==0?-.75f:.5f);
        controls.fxSend[static_cast<std::size_t>(channel)].store(.6f);
    }
    for(std::size_t route=0;route<controls.route.size();++route)
        controls.route[route].store(route%3==0?-.25f:.75f);
    controls.delayReturn.store(.3f);controls.reverbReturn.store(.7f);
    controls.fxDelayMs.store(437);controls.fxFeedback.store(.42f);
    controls.referenceChannel.store(6);controls.measurementChannel.store(7);
}

std::vector<float> snapshot(lah::dsp::Controls& controls,bool includeProfile){
    std::vector<float> result;
    for(auto* field:retainedDspFields(controls))
        for(const auto& value:*field)result.push_back(value.load());
    result.push_back(controls.fxDelayMs.load());result.push_back(controls.fxFeedback.load());
    result.push_back(static_cast<float>(controls.referenceChannel.load()));
    result.push_back(static_cast<float>(controls.measurementChannel.load()));
    if(includeProfile){
        for(const auto& value:controls.route)result.push_back(value.load());
        for(const auto& value:controls.inputPan)result.push_back(value.load());
        for(const auto& value:controls.fxSend)result.push_back(value.load());
        result.push_back(controls.delayReturn.load());result.push_back(controls.reverbReturn.load());
    }
    return result;
}
}

int main(){
    lah::dsp::Controls controls;
    for(int channelCount=1;channelCount<=8;++channelCount){
        seedControls(controls);const auto preserved=snapshot(controls,false);
        REQUIRE(lah::audio::applyUsbReturnProfile(controls,channelCount));
        int activeRoutes=0;
        for(int output=0;output<lah::dsp::channels;++output)
            for(int input=0;input<lah::dsp::channels;++input){
                const float actual=controls.route[static_cast<std::size_t>(output*lah::dsp::channels+input)].load();
                if(output==input && input<channelCount){REQUIRE(actual==1);++activeRoutes;}
                else REQUIRE(actual==0);
            }
        REQUIRE(activeRoutes==channelCount);
        for(int channel=0;channel<lah::dsp::channels;++channel){
            REQUIRE(controls.inputPan[static_cast<std::size_t>(channel)].load()==0);
            REQUIRE(controls.fxSend[static_cast<std::size_t>(channel)].load()==0);
        }
        REQUIRE(controls.delayReturn.load()==0 && controls.reverbReturn.load()==0);
        REQUIRE(snapshot(controls,false)==preserved);
        const auto applied=snapshot(controls,true);
        REQUIRE(lah::audio::applyUsbReturnProfile(controls,channelCount));
        REQUIRE(snapshot(controls,true)==applied); // applying a profile twice is stable
    }
    for(int invalid:{std::numeric_limits<int>::min(),-1,0,9,std::numeric_limits<int>::max()}){
        seedControls(controls);const auto before=snapshot(controls,true);
        REQUIRE(!lah::audio::applyUsbReturnProfile(controls,invalid));
        REQUIRE(snapshot(controls,true)==before);
    }
    std::cout << "PASS: USB profiles 1-8, exact 32x32 routing, shared FX reset, retained DSP/faders, invalid counts leave controls unchanged\n";
}
