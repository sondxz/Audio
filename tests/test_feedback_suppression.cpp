#include "../src/dsp/FeedbackSuppressionCore.h"
#include "../src/dsp/BuiltinRackCore.h"
#include "../src/dsp/BuiltinSceneState.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <vector>

namespace { bool countAllocations=false; std::size_t allocations=0; }
void* operator new(std::size_t n) { if(countAllocations)++allocations; if(auto* p=std::malloc(n?n:1))return p; throw std::bad_alloc(); }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
void operator delete[](void* p,std::size_t) noexcept { std::free(p); }
#define REQUIRE(x) do { if(!(x)){std::cerr<<"FAILED "<<__LINE__<<": "<<#x<<'\n';std::exit(1);} } while(false)
namespace {
constexpr double pi=3.14159265358979323846;
using lah::feedback::Processor;
using lah::feedback::Settings;
using lah::feedback::Status;
float sinusoid(int n,double rate,double hz,double amplitude=.2) {
    return static_cast<float>(amplitude*std::sin(2*pi*hz*n/rate));
}
// This source is an exponentially growing tone, not an acoustic ground truth.
float growing(int n,double rate,double hz) {
    const double amplitude=std::min(.25,.002*std::pow(10.0,static_cast<double>(n)/rate*12.0/20.0));
    return sinusoid(n,rate,hz,amplitude);
}
Settings armedSettings(bool useModel=false) {
    Settings settings;settings.armed=true;settings.useModel=useModel;settings.persistenceMs=350;
    settings.modelThreshold=.9f;settings.maxDepthDb=12;settings.q=30;return settings;
}
void requireBoundedStatus(const Status& status) {
    REQUIRE(status.notchCount>=0&&status.notchCount<=6);
    REQUIRE(std::isfinite(status.candidateHz)&&std::isfinite(status.modelScore));
    double totalDepth=0;
    for(int i=0;i<status.notchCount;++i){
        REQUIRE(std::isfinite(status.notchHz[static_cast<std::size_t>(i)]));
        REQUIRE(status.notchHz[static_cast<std::size_t>(i)]>0);
        REQUIRE(status.depthDb[static_cast<std::size_t>(i)]<=0&&status.depthDb[static_cast<std::size_t>(i)]>=-12.001f);
        totalDepth-=status.depthDb[static_cast<std::size_t>(i)];
    }
    REQUIRE(totalDepth<=36.001);
}
void testUnarmedIdentityAndNoLookahead() {
    auto processor=std::make_unique<Processor>();processor->prepare(48000);
    processor->configure(Settings{});
    // The very first sample is returned at the same index; analysis cannot add audio buffering.
    REQUIRE(processor->processSample(.375f)==.375f);
    for(int n=0;n<144000;++n){const auto x=growing(n,48000,1234.56);REQUIRE(processor->processSample(x)==x);}
    REQUIRE(processor->status().notchCount==0);
    processor->reset();REQUIRE(processor->status().notchCount==0);
}
void testSteadyAndNegativeSources() {
    auto processor=std::make_unique<Processor>();processor->prepare(48000);
    processor->configure(armedSettings());
    // Persistent pure music/test sine is ambiguous and must not be cut without growth.
    for(int n=0;n<144000;++n)REQUIRE(processor->processSample(sinusoid(n,48000,1000))==sinusoid(n,48000,1000));
    REQUIRE(processor->status().notchCount==0);
    processor->reset();processor->configure(armedSettings());
    std::uint32_t random=0x639712U;
    for(int n=0;n<144000;++n){
        random^=random<<13;random^=random>>17;random^=random<<5;
        const float noise=.1f*(static_cast<float>(random)/4294967295.0f*2-1);
        REQUIRE(processor->processSample(noise)==noise);
    }
    REQUIRE(processor->status().notchCount==0);
    processor->reset();processor->configure(armedSettings());
    for(int n=0;n<144000;++n){
        const auto x=sinusoid(n,48000,220,.08)+sinusoid(n,48000,440,.06)+sinusoid(n,48000,660,.04);
        REQUIRE(std::isfinite(processor->processSample(x)));
    }
    REQUIRE(processor->status().notchCount==0);
    processor->reset();processor->configure(armedSettings());
    for(int n=0;n<144000;++n){
        const auto x=static_cast<float>(.2*std::sin(2*pi*880*n/48000+3*std::sin(2*pi*6*n/48000)));
        REQUIRE(std::isfinite(processor->processSample(x)));
    }
    REQUIRE(processor->status().notchCount==0);
    processor->reset();processor->configure(armedSettings());
    for(int n=0;n<144000;++n){
        const auto x=n<4800?sinusoid(n,48000,1800,.4):0;
        REQUIRE(std::isfinite(processor->processSample(x)));
    }
    REQUIRE(processor->status().notchCount==0);
}
void testSuppressionAndReset(double rate,double hz) {
    auto processor=std::make_unique<Processor>();processor->prepare(rate);
    processor->configure(armedSettings());
    int firstCut=-1;double dryEnergy=0,wetEnergy=0;
    const int length=static_cast<int>(rate*6);
    for(int n=0;n<length;++n){
        const float x=growing(n,rate,hz),y=processor->processSample(x);
        REQUIRE(std::isfinite(y)&&std::abs(y)<=16);
        const auto state=processor->status();requireBoundedStatus(state);
        if(firstCut<0&&state.notchCount>0)firstCut=n;
        if(n>length-static_cast<int>(rate*.5)){dryEnergy+=x*x;wetEnergy+=y*y;}
    }
    REQUIRE(firstCut>static_cast<int>(rate*.1)&&firstCut<static_cast<int>(rate*4));
    const auto state=processor->status();REQUIRE(state.notchCount>=1);
    bool found=false;
    for(int i=0;i<state.notchCount;++i)found=found||std::abs(state.notchHz[static_cast<std::size_t>(i)]-hz)<std::max(8.0,hz*.01);
    REQUIRE(found);
    const double attenuation=10*std::log10(std::max(1e-30,wetEnergy)/dryEnergy);
    REQUIRE(attenuation<-7);
    std::cout<<"growth tone "<<hz<<" Hz @ "<<rate<<": first cut "<<1000*firstCut/rate<<" ms; attenuation "<<attenuation<<" dB\n";
    auto disarmed=armedSettings();disarmed.armed=false;processor->configure(disarmed);
    REQUIRE(!processor->status().armed&&processor->status().notchCount==state.notchCount);
    // DISARM freezes learned filters. Explicit clear increments its generation
    // and removes the existing wet correction over at most 64 samples.
    for(int n=0;n<1000;++n)(void)processor->processSample(sinusoid(n,rate,hz));
    ++disarmed.resetGeneration;processor->configure(disarmed);
    REQUIRE(processor->status().notchCount==0);
    for(int n=0;n<64;++n)REQUIRE(std::isfinite(processor->processSample(.123f)));
    REQUIRE(processor->processSample(.123f)==.123f);
    processor->reset();const auto reset=processor->status();REQUIRE(reset.notchCount==0&&reset.candidateHz==0);
    for(int n=0;n<8192;++n)REQUIRE(processor->processSample(0)==0);
    // Reset disarms and clears state; no stale cut can resume on new audio.
    REQUIRE(!processor->status().armed&&processor->processSample(.123f)==.123f);
}
void testLearnedGateClosedLoop() {
    // A narrowband recursive TEST PLANT, not a real microphone/room model.
    // The independent Python suite verifies this plant using transfer polynomials.
    constexpr int rate=48000,delay=48,frames=rate*6;
    const double w=2*pi*1000/rate,alpha=std::sin(w)/40,scale=1+alpha;
    const double b0=alpha/scale,b2=-b0,a1=-2*std::cos(w)/scale,a2=(1-alpha)/scale;
    double uncontrolledTail=0,controlledTail=0;
    for(bool active:{false,true}){
        auto processor=std::make_unique<Processor>();processor->prepare(rate);
        auto settings=armedSettings(true);settings.armed=active;processor->configure(settings);
        std::array<float,delay> loop{};int position=0;double z1=0,z2=0,tail=0;
        float maximum=0,maximumScore=0;
        for(int n=0;n<frames;++n){
            const double v=loop[static_cast<std::size_t>(position)],feedback=b0*v+z1;
            z1=-a1*feedback+z2;z2=b2*v-a2*feedback;
            const double seed=n<rate/10?sinusoid(n,rate,1000,.001):0;
            const float x=static_cast<float>(std::clamp(seed+1.08*feedback,-8.,8.));
            const float y=processor->processSample(x);REQUIRE(std::isfinite(y));
            loop[static_cast<std::size_t>(position)]=y;if(++position==delay)position=0;
            maximum=std::max(maximum,std::abs(y));maximumScore=std::max(maximumScore,processor->status().modelScore);
            if(n>frames-rate/2)tail+=y*y;
        }
        if(active){
            REQUIRE(processor->status().notchCount>=1);REQUIRE(maximumScore>=settings.modelThreshold);
            REQUIRE(maximum<1);controlledTail=tail;
        }else{REQUIRE(processor->status().notchCount==0);uncontrolledTail=tail;}
    }
    REQUIRE(uncontrolledTail>1&&controlledTail<.01*uncontrolledTail);
}
void testIsolationFiniteAndAllocation() {
    auto left=std::make_unique<Processor>(),right=std::make_unique<Processor>();
    left->prepare(48000);right->prepare(48000,128);
    left->configure(armedSettings());right->configure(Settings{});
    allocations=0;countAllocations=true;
    for(int n=0;n<192000;++n){
        const float x=growing(n,48000,1357.9),y=left->processSample(x);
        REQUIRE(std::isfinite(y));REQUIRE(right->processSample(x)==x);
        if(n%128==0){left->configure(armedSettings());requireBoundedStatus(left->status());}
    }
    left->reset();right->reset();countAllocations=false;REQUIRE(allocations==0);
    for(double rate:{8000.,44100.,48000.,96000.,192000.}){
        left->prepare(rate);auto settings=armedSettings();
        settings.maxDepthDb=std::numeric_limits<float>::quiet_NaN();settings.q=std::numeric_limits<float>::infinity();
        settings.persistenceMs=-1;settings.modelThreshold=std::numeric_limits<float>::quiet_NaN();
        left->configure(settings);
        for(float x:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),std::numeric_limits<float>::max()})
            REQUIRE(std::isfinite(left->processSample(x)));
        for(int n=0;n<8192;++n)REQUIRE(std::isfinite(left->processSample(0)));
        requireBoundedStatus(left->status());
    }
}
void testRackArmInvalidationAndTelemetry() {
    auto rack=std::make_unique<lah::builtin::Rack>();
    auto controls=std::make_unique<lah::builtin::Controls>();
    rack->prepare(48000,128);
    auto& slot=controls->slots[0][0];lah::builtin::configureSlot(slot,lah::builtin::AutoFeedback,true);
    slot.params[3].store(350);rack->beginBlock(*controls);
    slot.feedbackArmed.store(true); // arm after the new module's initial callback reset
    allocations=0;countAllocations=true;
    for(int n=0;n<192000;++n){
        if(n%128==0)rack->beginBlock(*controls);
        const float x=growing(n,48000,1357.9),y=rack->processSample(0,x);
        REQUIRE(std::isfinite(y));REQUIRE(rack->processSample(1,x)==x);
    }
    countAllocations=false;REQUIRE(allocations==0);
    REQUIRE(rack->feedbackStatus(0,0).notchCount>=1);
    REQUIRE(rack->feedbackStatus(1,0).notchCount==0);
    rack->reset();REQUIRE(rack->feedbackStatus(0,0).notchCount==0);
    rack->beginBlock(*controls);REQUIRE(!slot.feedbackArmed.load());
    REQUIRE(rack->processSample(0,0)==0&&rack->feedbackStatus(0,0).notchCount==0);
    slot.feedbackArmed.store(true);rack->beginBlock(*controls);
    rack->resetChannel(0);REQUIRE(rack->feedbackStatus(0,0).notchCount==0);
    REQUIRE(rack->processSample(0,0)==0&&!slot.feedbackArmed.load());
    slot.feedbackArmed.store(true);rack->beginBlock(*controls);
    slot.enabled.store(false);rack->beginBlock(*controls);
    REQUIRE(!slot.feedbackArmed.load()&&rack->feedbackStatus(0,0).notchCount==0);
    slot.enabled.store(true);rack->beginBlock(*controls);
    for(int n=0;n<1000;++n)REQUIRE(rack->processSample(0,0)==0);
    REQUIRE(!slot.feedbackArmed.load()&&rack->feedbackStatus(0,0).notchCount==0);
    slot.feedbackArmed.store(true);
    for(int n=0;n<192000;++n){
        if(n%128==0)rack->beginBlock(*controls);
        (void)rack->processSample(0,growing(n,48000,1357.9));
    }
    REQUIRE(rack->feedbackStatus(0,0).notchCount>=1);
    const auto saved=lah::builtin::snapshot(*controls);
    const auto beforeGeneration=slot.feedbackResetGeneration.load();
    lah::builtin::recall(*controls,saved);
    REQUIRE(!slot.feedbackArmed.load()&&slot.feedbackResetGeneration.load()!=beforeGeneration);
    // Recall may restore exactly the same enabled/module values before the
    // next callback. Its generation must still invalidate all learned cuts.
    rack->beginBlock(*controls);
    for(int n=0;n<64;++n)REQUIRE(std::isfinite(rack->processSample(0,0)));
    REQUIRE(rack->feedbackStatus(0,0).notchCount==0&&!rack->feedbackStatus(0,0).armed);
}
}
int main() {
    testUnarmedIdentityAndNoLookahead();testSteadyAndNegativeSources();
    for(double rate:{44100.,48000.,96000.}){testSuppressionAndReset(rate,1000);testSuppressionAndReset(rate,1234.56);}
    testLearnedGateClosedLoop();
    testIsolationFiniteAndAllocation();
    testRackArmInvalidationAndTelemetry();
    std::cout<<"PASS: automatic feedback suppression growth/steady/noise/harmonic/vibrato/transient, exact dry path, reset, isolation, bounds and zero callback allocations (synthetic only)\n";
}
