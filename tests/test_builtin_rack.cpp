#include "../src/dsp/BuiltinRackCore.h"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>

namespace {bool countAllocations=false;std::size_t allocations=0;}
void* operator new(std::size_t n){if(countAllocations)++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
#define REQ(x) do{if(!(x)){std::cerr<<"FAILED "<<__LINE__<<": "<<#x<<'\n';std::exit(1);}}while(false)
using namespace lah::builtin;

int main(){
    Controls controls;
    Rack rack;
    for(int type=1;type<=moduleCount;++type){
        const auto& info=metadata(type);REQ(info.name&&info.description&&info.parameterCount==6);
        for(const auto& p:info.params){REQ(p.name&&p.unit&&p.minimum<=p.defaultValue&&p.defaultValue<=p.maximum&&p.step>0);}
    }
    bool rejected=false;try{rack.prepare(std::numeric_limits<double>::quiet_NaN());}catch(const std::invalid_argument&){rejected=true;}REQ(rejected);
    rack.prepare(48000);rack.beginBlock(controls);
    for(float x:{-.8f,-.1f,0.0f,.1f,.8f})for(int ch=0;ch<32;++ch)REQ(rack.processSample(ch,x)==x);
    REQ(rack.processSample(-1,1)==0&&rack.processSample(32,1)==0);
    REQ(rack.processSample(0,std::numeric_limits<float>::quiet_NaN())==0);
    REQ(rack.processSample(0,std::numeric_limits<float>::max())==std::numeric_limits<float>::max());

    // Every defined module processes independent channels, finite extremes and silence without allocation.
    countAllocations=true;
    for(int type=1;type<=moduleCount;++type){
        configureSlot(controls.slots[0][0],type,true);rack.reset();rack.beginBlock(controls);
        for(int frame=0;frame<12000;++frame){
            const float x=frame==0?.7f:.15f*std::sin(static_cast<float>(2*pi*997*frame/48000));
            const float y=rack.processSample(0,x);REQ(std::isfinite(y)&&std::abs(y)<=16);
            REQ(rack.processSample(1,x)==x);
        }
        for(auto& p:controls.slots[0][0].params)p.store(std::numeric_limits<float>::quiet_NaN());
        rack.beginBlock(controls);REQ(std::isfinite(rack.processSample(0,std::numeric_limits<float>::max())));
        rack.reset();rack.beginBlock(controls);
        for(int frame=0;frame<1000;++frame)REQ(rack.processSample(0,0)==0);
    }
    countAllocations=false;REQ(allocations==0);

    // Analytic static compression target: threshold * (input/threshold)^(1/ratio).
    configureSlot(controls.slots[0][0],Dynamics,true);auto& slot=controls.slots[0][0];
    slot.params[0].store(-20);slot.params[1].store(4);slot.params[4].store(-100);
    rack.reset();rack.beginBlock(controls);float compressed=0;
    for(int n=0;n<96000;++n)compressed=rack.processSample(0,.5f);
    const float expected=.1f*std::pow(5.0f,.25f);REQ(std::abs(compressed-expected)<3e-5f);
    // Enable/bypass transition is bounded and returns to exact dry in 64 samples.
    slot.enabled.store(false);rack.beginBlock(controls);float previous=compressed;
    for(int n=0;n<64;++n){const float y=rack.processSample(0,.5f);REQ(std::abs(y-previous)<.006f);previous=y;}
    REQ(rack.processSample(0,.5f)==.5f);
    for(int change=0;change<40;++change){
        slot.enabled.store((change&1)!=0);rack.beginBlock(controls);
        for(int n=0;n<17;++n){const float y=rack.processSample(0,.5f);REQ(std::isfinite(y)&&std::abs(y-previous)<.02f);previous=y;}
    }

    // Slot ordering is real: pre-compression gain and post-compression gain differ.
    configureSlot(slot,VocalColor,true);slot.params[4].store(0);slot.params[5].store(6);
    configureSlot(controls.slots[0][1],Dynamics,true);
    controls.slots[0][1].params[0].store(-20);controls.slots[0][1].params[1].store(4);
    rack.reset();rack.beginBlock(controls);float orderA=0,orderB=0;
    for(int n=0;n<96000;++n)orderA=rack.processSample(0,.2f);
    configureSlot(slot,Dynamics,true);slot.params[0].store(-20);slot.params[1].store(4);
    configureSlot(controls.slots[0][1],VocalColor,true);
    controls.slots[0][1].params[4].store(0);controls.slots[0][1].params[5].store(6);
    rack.reset();rack.beginBlock(controls);
    for(int n=0;n<96000;++n)orderB=rack.processSample(0,.2f);
    REQ(std::abs(orderA-orderB)>.05f);
    configureSlot(controls.slots[0][1],None,false);

    // Native delay has its first echo exactly at rounded lag, then feedback echoes.
    configureSlot(slot,Fx,true);slot.params[0].store(100);slot.params[1].store(10);slot.params[2].store(0);
    rack.reset();rack.beginBlock(controls);
    REQ(rack.processSample(0,.5f)==0);
    for(int n=1;n<480;++n)REQ(rack.processSample(0,0)==0);
    REQ(std::abs(rack.processSample(0,0)-.5f)<1e-6f);
    // Panic and channel mute invalidation discard all delay/reverb samples without bulk clearing.
    for(int type:{Fx,DigitalFx,Reverb96}){
        configureSlot(slot,type,true);rack.reset();rack.beginBlock(controls);
        for(int n=0;n<20000;++n)(void)rack.processSample(0,n==0?.5f:0);
        rack.resetChannel(0);
        for(int n=0;n<40000;++n)REQ(rack.processSample(0,0)==0);
    }
    // A tone advisory is measurable, but never triggers an automatic cut. Broadband is rejected.
    configureSlot(slot,AutoFeedback,true);rack.reset();rack.beginBlock(controls);
    for(int n=0;n<96000;++n){const float x=.2f*std::sin(static_cast<float>(2*pi*1000*n/48000));REQ(std::abs(rack.processSample(0,x)-x)<1e-6f);}
    REQ(std::abs(rack.feedbackCandidateHz(0,0)-1000)<10);
    rack.reset();REQ(rack.feedbackCandidateHz(0,0)==0);
    rack.beginBlock(controls);std::uint32_t random=0x98765431;
    for(int n=0;n<96000;++n){random^=random<<13;random^=random>>17;random^=random<<5;(void)rack.processSample(0,.3f*(static_cast<float>(random)/4294967295.0f*2-1));}
    REQ(rack.feedbackCandidateHz(0,0)==0);
    for(int n=0;n<10000;++n)(void)rack.processSample(0,0);
    REQ(rack.feedbackCandidateHz(0,0)==0);

    // Maximum-rate storage and extreme metadata ranges remain bounded and finite.
    rack.prepare(192000);
    for(int type=1;type<=moduleCount;++type){
        configureSlot(slot,type,true);const auto& info=metadata(type);
        for(int n=0;n<6;++n)slot.params[static_cast<std::size_t>(n)].store(info.params[static_cast<std::size_t>(n)].maximum);
        rack.reset();rack.beginBlock(controls);
        for(int n=0;n<2000;++n)REQ(std::isfinite(rack.processSample(0,n==0?1.0f:0.0f)));
    }
    std::cout<<"PASS: 15 native modules, metadata, finite/range guards, identity bypass, channel isolation, zero callback allocations, compressor law, delay timing, panic/mute epochs, transition ramp and tone-vs-noise advisory\n";
}
