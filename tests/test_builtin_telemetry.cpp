#include "../src/dsp/BuiltinRackCore.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>

#define REQUIRE(condition) do { if (!(condition)) { std::cerr << "FAILED line " << __LINE__ << ": " #condition "\n"; std::exit(1); } } while(false)

namespace {
void requireLevels(lah::builtin::MeterLevels meter,float input,float output){
    REQUIRE(std::isfinite(meter.inputPeak)&&std::isfinite(meter.outputPeak));
    REQUIRE(std::abs(meter.inputPeak-input)<2e-6f);
    REQUIRE(std::abs(meter.outputPeak-output)<2e-6f);
}
void configureGain(lah::builtin::SlotControl& slot,float db){
    lah::builtin::configureSlot(slot,lah::builtin::Dynamics,true);
    slot.params[0].store(0);slot.params[1].store(1);slot.params[4].store(-100);slot.params[5].store(db);
}
void testFeedbackVisibility(){
    auto rack=std::make_unique<lah::builtin::Rack>();lah::builtin::Controls controls;
    auto& slot=controls.slots[7][0];lah::builtin::configureSlot(slot,lah::builtin::AutoFeedback,true);
    slot.params[3].store(350);rack->prepare(48000);rack->beginBlock(controls);
    rack->processSample(7,0);slot.feedbackArmed.store(true);
    auto tone=[](int frame){
        const double amplitude=std::min(.25,.002*std::pow(10.,frame/48000.*12/20));
        return static_cast<float>(amplitude*std::sin(2*lah::builtin::pi*1234.56*frame/48000.));
    };
    for(int block=0;block<1125;++block){
        rack->beginBlock(controls);
        for(int frame=0;frame<128;++frame)rack->processSample(7,tone(block*128+frame));
    }
    REQUIRE(rack->feedbackStatus(7,0).notchCount>0);
    REQUIRE(rack->feedbackCandidateHz(7,0)>0);
    rack->invalidateMeters(); // the atomic invalidation used by transport MUTE
    REQUIRE(rack->feedbackStatus(7,0).notchCount==0);
    REQUIRE(rack->feedbackStatus(7,0).candidateHz==0);
    REQUIRE(rack->feedbackCandidateHz(7,0)==0);
    // Finish an already-started callback after the control thread invalidated
    // visibility. New analysis revisions must retain that callback's old epoch.
    for(int frame=144000;frame<146048;++frame){
        rack->processSample(7,tone(frame));
        REQUIRE(rack->feedbackStatus(7,0).notchCount==0);
        REQUIRE(rack->feedbackCandidateHz(7,0)==0);
    }
    rack->reset();rack->beginBlock(controls);rack->processSample(7,0);
    REQUIRE(rack->feedbackStatus(7,0).notchCount==0);
    REQUIRE(rack->feedbackCandidateHz(7,0)==0);
}
}

int main(){
    auto rack=std::make_unique<lah::builtin::Rack>();lah::builtin::Controls controls;
    configureGain(controls.slots[7][0],6);configureGain(controls.slots[7][2],-6);
    rack->prepare(48000);
    requireLevels(rack->meterLevels(7,0),0,0);
    REQUIRE(rack->meterLevels(7,0).generation==0);
    auto process=[&](float selected,float other){
        rack->beginBlock(controls);
        for(int frame=0;frame<128;++frame){rack->processSample(7,selected);rack->processSample(0,other);}
    };
    process(.1f,-.4f);
    requireLevels(rack->meterLevels(7,0),0,0); // publishes completed previous blocks only
    process(.1f,-.4f);
    const float boosted=.1f*lah::builtin::gain(6);
    requireLevels(rack->meterLevels(7,0),.1f,boosted);
    requireLevels(rack->meterLevels(7,1),boosted,boosted); // empty slot is measured dry
    requireLevels(rack->meterLevels(7,2),boosted,.1f);
    requireLevels(rack->meterLevels(7,3),.1f,.1f);
    requireLevels(rack->meterLevels(0,0),.4f,.4f); // selected channel is isolated
    requireLevels(rack->meterLevels(6,0),0,0); // unprocessed channels have no invented levels
    REQUIRE(rack->meterLevels(6,0).generation==0);
    const auto measured=rack->meterLevels(7,0);
    REQUIRE(measured.generation!=0);
    REQUIRE(rack->meterLevels(7,0).generation==measured.generation); // reads do not consume

    // After the bypass transition settles, a bypassed processor reports the
    // actual equal dry/wet levels rather than a model of its enabled algorithm.
    controls.slots[7][0].enabled.store(false);
    process(.1f,-.4f);process(.1f,-.4f);process(.1f,-.4f);
    requireLevels(rack->meterLevels(7,0),.1f,.1f);

    rack->resetChannel(7);
    requireLevels(rack->meterLevels(7,0),0,0);
    requireLevels(rack->meterLevels(0,0),.4f,.4f); // resetting one channel leaves the other valid
    process(0,-.4f);process(0,-.4f);
    requireLevels(rack->meterLevels(7,0),0,0);

    process(.2f,-.4f);process(.2f,-.4f);
    requireLevels(rack->meterLevels(7,0),.2f,.2f);
    rack->invalidateMeters();
    requireLevels(rack->meterLevels(7,0),0,0);
    REQUIRE(rack->meterLevels(7,0).generation==0);
    process(0,0); // the pre-invalidation block may never reappear
    requireLevels(rack->meterLevels(7,0),0,0);
    process(0,0);requireLevels(rack->meterLevels(7,0),0,0);

    process(.3f,0);process(.3f,0);rack->reset();
    requireLevels(rack->meterLevels(7,0),0,0);
    process(0,0);process(0,0);requireLevels(rack->meterLevels(7,0),0,0);
    process(.3f,0);process(.3f,0);rack->prepare(44100);
    requireLevels(rack->meterLevels(7,0),0,0); // device reprepare invalidates all readouts
    process(0,0);process(0,0);requireLevels(rack->meterLevels(7,0),0,0);

    for(int channel:{-1,32,std::numeric_limits<int>::max()})requireLevels(rack->meterLevels(channel,0),0,0);
    for(int slot:{-1,4,std::numeric_limits<int>::max()})requireLevels(rack->meterLevels(0,slot),0,0);
    process(std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity());
    process(0,0);requireLevels(rack->meterLevels(7,0),0,0);requireLevels(rack->meterLevels(0,0),0,0);
    testFeedbackVisibility();
    std::cout << "PASS: actual per-slot pre/post peaks, channel 8 and bypass, read-only generations, reset/channel/PANIC epochs, reprepare, finite/bounds guards\n";
}
