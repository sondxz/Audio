#include "../src/dsp/SmartCompMacro.h"
#include "../src/dsp/BuiltinSceneState.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>

#define REQUIRE(condition) do { if (!(condition)) { std::cerr << "FAILED line " << __LINE__ << ": " #condition "\n"; std::exit(1); } } while(false)

namespace {
using namespace lah::builtin;
void expect(const SmartCompMacroParameters& actual,const SmartCompMacroParameters& expected){
    for(std::size_t p=0;p<actual.size();++p)REQUIRE(std::abs(actual[p]-expected[p])<1e-5f);
}
double settledGain(float macro,float amplitude){
    auto rack=std::make_unique<Rack>();Controls controls;
    configureSlot(controls.slots[0][0],SmartComp,true);REQUIRE(applySmartCompMacro(controls.slots[0][0],macro));
    rack->prepare(48000);
    double inputEnergy=0,outputEnergy=0;
    for(int block=0;block<800;++block){
        rack->beginBlock(controls);
        for(int frame=0;frame<128;++frame){
            const int index=block*128+frame;
            const float input=amplitude*static_cast<float>(std::sin(2*pi*997*index/48000.));
            const float output=rack->processSample(0,input);
            REQUIRE(std::isfinite(output));
            if(block>=600){inputEnergy+=input*input;outputEnergy+=output*output;}
        }
    }
    return std::sqrt(outputEnergy/inputEnergy);
}
}

int main(){
    using namespace lah::builtin;
    expect(mapSmartCompMacro(-100),{-12,1.5f,20,120,100,6});
    expect(mapSmartCompMacro(0),{-18,3,10,200,100,0});
    expect(mapSmartCompMacro(100),{-30,6,5,400,100,0});
    expect(mapSmartCompMacro(-500),mapSmartCompMacro(-100));expect(mapSmartCompMacro(500),mapSmartCompMacro(100));
    for(float invalid:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()})
        expect(mapSmartCompMacro(invalid),mapSmartCompMacro(0));
    const auto& info=metadata(SmartComp);
    for(int amount=-100;amount<=100;++amount){
        const auto mapped=mapSmartCompMacro(static_cast<float>(amount));
        for(std::size_t p=0;p<mapped.size();++p){REQUIRE(std::isfinite(mapped[p]));REQUIRE(mapped[p]>=info.params[p].minimum&&mapped[p]<=info.params[p].maximum);}
        const auto inferred=inferSmartCompMacro(mapped);REQUIRE(inferred.matches&&std::abs(inferred.amount-amount)<.001f);
    }
    const auto left=mapSmartCompMacro(-.001f),right=mapSmartCompMacro(.001f);
    for(std::size_t p=0;p<left.size();++p)REQUIRE(std::abs(left[p]-right[p])<.003f);
    for(std::size_t changed=0;changed<6;++changed){
        auto custom=mapSmartCompMacro(0);custom[changed]+=.1f;REQUIRE(!inferSmartCompMacro(custom).matches);
    }
    auto invalid=mapSmartCompMacro(0);invalid[2]=std::numeric_limits<float>::quiet_NaN();REQUIRE(!inferSmartCompMacro(invalid).matches);
    invalid=mapSmartCompMacro(0);invalid[0]=-80;REQUIRE(!inferSmartCompMacro(invalid).matches);

    Controls controls;configureSlot(controls.slots[7][3],SmartComp,true);
    configureSlot(controls.slots[6][2],VocalColor,true);controls.slots[6][2].params[1].store(7);
    const auto before=snapshot(controls);
    REQUIRE(applySmartCompMacro(controls.slots[7][3],42));
    REQUIRE(controls.slots[7][3].type.load()==SmartComp&&controls.slots[7][3].enabled.load());
    REQUIRE(inferSmartCompMacro(controls.slots[7][3]).matches);
    REQUIRE(std::abs(inferSmartCompMacro(controls.slots[7][3]).amount-42)<.001f);
    REQUIRE(!applySmartCompMacro(controls.slots[6][2],100));
    const auto after=snapshot(controls);
    for(int channel=0;channel<channelCount;++channel)for(int slot=0;slot<slotCount;++slot){
        if(channel==7&&slot==3)continue;
        const auto& a=before[static_cast<std::size_t>(channel)][static_cast<std::size_t>(slot)];
        const auto& b=after[static_cast<std::size_t>(channel)][static_cast<std::size_t>(slot)];
        REQUIRE(a.type==b.type&&a.enabled==b.enabled&&a.params==b.params);
    }
    // A legacy custom scene remains six unchanged parameters after inference.
    controls.slots[7][3].params[5].store(4.2f);const auto legacy=snapshot(controls);
    REQUIRE(!inferSmartCompMacro(controls.slots[7][3]).matches);
    const auto untouched=snapshot(controls);REQUIRE(legacy[7][3].params==untouched[7][3].params);
    Controls restored;recall(restored,legacy);REQUIRE(snapshot(restored)[7][3].params==legacy[7][3].params);
    configureSlot(restored.slots[7][3],SmartComp,true);REQUIRE(inferSmartCompMacro(restored.slots[7][3]).matches);
    REQUIRE(inferSmartCompMacro(restored.slots[7][3]).amount==0); // old default scenes are Balanced

    // COMP changes compression, not just makeup. At both low/high input levels
    // makeup remains zero; only the above-threshold signal loses substantial gain.
    const double balancedLow=settledGain(0,.005f),compressedLow=settledGain(100,.005f);
    const double balancedHigh=settledGain(0,.5f),compressedHigh=settledGain(100,.5f);
    REQUIRE(std::abs(balancedLow-1)<1e-5&&std::abs(compressedLow-1)<1e-5);
    REQUIRE(compressedHigh<balancedHigh*.5);
    std::cout << "PASS: SmartComp native macro mapping/defaults/continuity/bounds/inference, slot+legacy scene preservation, actual level-dependent compression (not gain-only)\n";
}
