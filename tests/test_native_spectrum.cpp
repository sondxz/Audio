#include "../src/dsp/NativeSpectrum.h"
#include "../src/dsp/BuiltinRackCore.h"
#include <cstdlib>
#include <iostream>
#include <new>
#include <limits>

namespace {bool countAllocations=false;std::size_t allocations=0;}
void* operator new(std::size_t size){if(countAllocations)++allocations;if(auto* p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t size){return ::operator new(size);}
void operator delete(void* p)noexcept{std::free(p);}void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
#define REQUIRE(c) do{if(!(c)){std::cerr<<"FAIL line "<<__LINE__<<": " #c "\n";std::exit(1);}}while(false)
using namespace lah::spectrum;
namespace {
constexpr double pi=3.14159265358979323846;
void feed(Capture& capture,int count,double rate,int bin,float amplitude=.25f,bool output=false,int channel=7,int slot=2,float postGain=.5f){
    countAllocations=true;
    capture.beginBlock(output);
    for(int i=0;i<count;++i){const float sample=amplitude*static_cast<float>(std::sin(2*pi*bin*i/fftSize));capture.push(output,channel,slot,sample,sample*postGain);}
    countAllocations=false;
    (void)rate;
    capture.processPending();
}
int maximumBin(const std::array<float,binCount>& bins){return static_cast<int>(std::max_element(bins.begin()+1,bins.end())-bins.begin());}
void sineBinsAndLevels(){
    for(double rate:{8000.,44100.,48000.,96000.,192000.}){
        auto capture=std::make_unique<Capture>(false);capture->prepare(rate);auto handle=capture->acquire(false,7,2);Snapshot result;
        REQUIRE(handle);REQUIRE(!capture->read(handle,result));
        const int bin=rate==8000?256:43;
        feed(*capture,fftSize,rate,bin);REQUIRE(capture->read(handle,result));REQUIRE(result.hasData && result.signalPresent);REQUIRE(result.sampleRate==rate);
        REQUIRE(maximumBin(result.preDb)==bin);REQUIRE(maximumBin(result.postDb)==bin);
        REQUIRE(std::abs(result.preDb[static_cast<std::size_t>(bin)]-20*std::log10(.25))<.002);
        REQUIRE(std::abs(result.postDb[static_cast<std::size_t>(bin)]-result.preDb[static_cast<std::size_t>(bin)]-20*std::log10(.5))<.002);
        capture->release(handle);REQUIRE(!capture->read(handle,result));
    }
}
void epochAndSelection(){
    auto capture=std::make_unique<Capture>(false);capture->prepare(48000);Snapshot result;
    auto input=capture->acquire(false,7,2),same=capture->acquire(false,7,2),output=capture->acquire(true,7,2),other=capture->acquire(false,7,1);
    REQUIRE(input.index==same.index && input.token==same.token);REQUIRE(output.index!=input.index && other.index!=input.index);
    feed(*capture,fftSize,48000,32);REQUIRE(capture->read(input,result));REQUIRE(capture->read(same,result));REQUIRE(!capture->read(output,result));REQUIRE(!capture->read(other,result));
    capture->release(input);REQUIRE(capture->read(same,result));
    // PANIC/rearm between callbacks invalidates the old block immediately.
    capture->invalidateDomain(false);REQUIRE(!capture->read(same,result));
    for(int i=0;i<fftSize;++i)capture->push(false,7,2,.8f,.8f); // old in-flight callback: must be rejected
    capture->processPending();REQUIRE(!capture->read(same,result));
    feed(*capture,fftSize-1,48000,32);REQUIRE(!capture->read(same,result));
    capture->beginBlock(false);capture->push(false,7,2,0,0);capture->processPending();REQUIRE(capture->read(same,result));
    feed(*capture,fftSize,48000,64,.1f,true);REQUIRE(capture->read(output,result));REQUIRE(maximumBin(result.preDb)==64);
    capture->invalidateTarget(false,7,2);REQUIRE(!capture->read(same,result));REQUIRE(capture->read(output,result));
    feed(*capture,fftSize,48000,32);REQUIRE(capture->read(same,result));
    capture->prepare(96000);REQUIRE(!capture->read(same,result));REQUIRE(!capture->read(output,result));
    feed(*capture,fftSize,96000,128);REQUIRE(capture->read(same,result));REQUIRE(result.sampleRate==96000);
    const auto old=same;capture->release(same);auto reused=capture->acquire(false,0,0);REQUIRE(reused.index==old.index);REQUIRE(reused.token!=old.token);REQUIRE(!capture->read(old,result));REQUIRE(!capture->read(reused,result));
    feed(*capture,fftSize,96000,99,.2f,false,0,0);REQUIRE(capture->read(reused,result));REQUIRE(maximumBin(result.preDb)==99);
    capture->release(reused);capture->release(other);capture->release(output);
}
void invalidSilenceOverflow(){
    auto capture=std::make_unique<Capture>(false);Snapshot result;REQUIRE(!capture->acquire(false,-1,0));REQUIRE(!capture->acquire(false,32,0));REQUIRE(!capture->acquire(false,0,4));
    const auto handle=capture->acquire(false,0,0);feed(*capture,fftSize,48000,32,0,false,0,0);REQUIRE(capture->read(handle,result));REQUIRE(!result.signalPresent);REQUIRE(result.preDb[32]==-140);
    capture->beginBlock(false);capture->push(false,0,0,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity());capture->processPending();
    feed(*capture,fftSize,48000,32,.25f,false,0,0);REQUIRE(capture->read(handle,result));
    capture->beginBlock(false);for(int i=0;i<9000;++i)capture->push(false,0,0,.1f,.1f);REQUIRE(!capture->read(handle,result));capture->processPending();REQUIRE(!capture->read(handle,result));
    feed(*capture,fftSize,48000,81,.25f,false,0,0);REQUIRE(capture->read(handle,result));REQUIRE(maximumBin(result.preDb)==81);
    capture->prepare(0);REQUIRE(!capture->read(handle,result));feed(*capture,fftSize,0,32,.25f,false,0,0);REQUIRE(!capture->read(handle,result));
    REQUIRE(allocations==0);
}
void workerAndCapacity(){
    auto capture=std::make_unique<Capture>(true);std::array<Handle,maximumSubscriptions> handles{};
    for(int i=0;i<maximumSubscriptions;++i){handles[static_cast<std::size_t>(i)]=capture->acquire(false,i,0);REQUIRE(handles[static_cast<std::size_t>(i)]);}
    REQUIRE(!capture->acquire(true,0,0));Snapshot result;
    capture->beginBlock(false);for(int i=0;i<fftSize;++i){const float sample=.25f*static_cast<float>(std::sin(2*pi*77*i/fftSize));capture->push(false,0,0,sample,sample);}
    for(int i=0;i<100 && !capture->read(handles[0],result);++i)std::this_thread::sleep_for(std::chrono::milliseconds(5));
    REQUIRE(result.hasData && maximumBin(result.preDb)==77);
    std::this_thread::sleep_for(std::chrono::milliseconds(510));REQUIRE(!capture->read(handles[0],result));
    for(auto handle:handles)capture->release(handle);
}
void realRackHook(){
    for(bool output:{false,true}){
        auto capture=std::make_unique<Capture>(false);auto controls=std::make_unique<lah::builtin::Controls>();auto rack=std::make_unique<lah::builtin::Rack>();
        rack->setSpectrumTap(capture.get(),output);rack->prepare(48000);capture->prepare(48000);
        auto& slot=controls->slots[7][2];lah::builtin::configureSlot(slot,lah::builtin::VocalColor,true);slot.params[4].store(0);slot.params[5].store(-12);
        auto selected=capture->acquire(output,7,2),before=capture->acquire(output,7,1),unrelated=capture->acquire(output,6,2);Snapshot measured,dry;
        for(int block=0;block<40;++block){rack->beginBlock(*controls);for(int i=0;i<128;++i){const float x=.1f*static_cast<float>(std::sin(2*pi*43*(block*128+i)/fftSize));rack->processSample(7,x);}capture->processPending();}
        REQUIRE(capture->read(selected,measured));REQUIRE(capture->read(before,dry));REQUIRE(!capture->read(unrelated,dry));
        REQUIRE(std::abs(measured.postDb[43]-measured.preDb[43]+12)<.01);
        // Module replacement cannot keep a previously displayed spectrum.
        lah::builtin::configureSlot(slot,lah::builtin::Equalizer,true);rack->beginBlock(*controls);REQUIRE(!capture->read(selected,measured));
        for(int i=0;i<fftSize;++i)rack->processSample(7,.1f*static_cast<float>(std::sin(2*pi*64*i/fftSize)));capture->processPending();REQUIRE(capture->read(selected,measured));
        rack->invalidateMeters();REQUIRE(!capture->read(selected,measured));
        for(int i=0;i<fftSize;++i)rack->processSample(7,.1f);capture->processPending();REQUIRE(!capture->read(selected,measured));
        rack->beginBlock(*controls);for(int i=0;i<fftSize;++i)rack->processSample(7,.1f);capture->processPending();REQUIRE(capture->read(selected,measured));
        rack->resetChannel(7);REQUIRE(!capture->read(selected,measured));
    }
}
}
int main(){sineBinsAndLevels();epochAndSelection();invalidSilenceOverflow();workerAndCapacity();realRackHook();std::cout<<"PASS: actual input/output Rack pre/post Hann FFT bins/levels 8-192k, duplicate references, slot/domain isolation, stale PANIC/module/device epochs, overflow recovery, silence/invalid samples, zero callback allocations, background worker and stale timeout\n";}
