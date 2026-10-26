// Parameter automation acceptance against the real native DSP rack. No device is opened.
#include "../src/dsp/BuiltinRackCore.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <new>

namespace {
bool countAllocations=false;
std::size_t allocations=0;
}
void* operator new(std::size_t size) {
    if(countAllocations)++allocations;
    if(auto* memory=std::malloc(size?size:1))return memory;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size){return ::operator new(size);}
void operator delete(void* memory)noexcept{std::free(memory);}
void operator delete[](void* memory)noexcept{std::free(memory);}
void operator delete(void* memory,std::size_t)noexcept{std::free(memory);}
void operator delete[](void* memory,std::size_t)noexcept{std::free(memory);}

#define REQUIRE(condition) do { if(!(condition)) { countAllocations=false; \
    std::cerr<<"FAILED line "<<__LINE__<<": " #condition "\n";std::exit(1); } } while(false)

namespace {
using namespace lah::builtin;
constexpr int blockSize=128;

void processStressBlock(Rack& rack,Controls& controls,double rate,std::uint64_t& frameCount) {
    rack.beginBlock(controls);
    for(int sample=0;sample<blockSize;++sample,++frameCount){
        // A small impulse plus a multitone exercises the delay storage and IIR state.
        const double time=static_cast<double>(frameCount)/rate;
        const float input=static_cast<float>(.14*std::sin(2*pi*997*time)+.07*std::sin(2*pi*83*time)
            +.03*std::sin(2*pi*6157*time)+(frameCount%4096==0?.3:0));
        const float actual=rack.processSample(0,input);
        REQUIRE(std::isfinite(actual)&&std::abs(actual)<=16.0f);
        // Two untouched active EQ channels receive the same signal. Editing CH1 must
        // not change their independent state, coefficients, or sample histories.
        const float referenceA=rack.processSample(2,input);
        const float referenceB=rack.processSample(3,input);
        REQUIRE(std::isfinite(referenceA)&&referenceA==referenceB);
        REQUIRE(rack.processSample(31,input)==input);
    }
}

void allModulesLiveRanges() {
    auto rack=std::make_unique<Rack>();
    Controls controls;
    configureSlot(controls.slots[2][0],Equalizer,true);
    configureSlot(controls.slots[3][0],Equalizer,true);
    controls.slots[2][0].params[3].store(-7);
    controls.slots[3][0].params[3].store(-7);
    std::uint64_t frameCount=0;

    for(double rate:{8000.0,44100.0,48000.0,96000.0,192000.0}){
        rack->prepare(rate,blockSize);
        // Force metadata initialization outside the callback allocation check.
        for(int type=1;type<=moduleCount;++type)REQUIRE(metadata(type).parameterCount==6);
        countAllocations=true;
        for(int type=1;type<=moduleCount;++type){
            auto& slot=controls.slots[0][0];
            configureSlot(slot,type,true);
            rack->reset();
            const bool delayModule=type==Fx||type==DigitalFx||type==Reverb96;
            const int warmBlocks=delayModule?static_cast<int>(std::ceil(rate*.8/blockSize)):8;
            for(int block=0;block<warmBlocks;++block)processStressBlock(*rack,controls,rate,frameCount);
            const auto& info=metadata(type);
            // No mute, prepare, reset, module replacement, or bypass occurs during
            // this sequence: every edit is consumed at the next 128-frame boundary.
            for(int parameter=0;parameter<info.parameterCount;++parameter){
                const auto& specification=info.params[static_cast<std::size_t>(parameter)];
                for(float value:{specification.minimum,specification.maximum,specification.defaultValue}){
                    slot.params[static_cast<std::size_t>(parameter)].store(value,std::memory_order_release);
                    for(int block=0;block<24;++block)processStressBlock(*rack,controls,rate,frameCount);
                    REQUIRE(slot.enabled.load()&&slot.type.load()==type);
                }
            }
            // Corrupt control values must also be bounded by the same realtime
            // snapshot path, including integer bit-depth and delay-size parameters.
            for(float invalid:{std::numeric_limits<float>::quiet_NaN(),
                               std::numeric_limits<float>::infinity(),
                               -std::numeric_limits<float>::infinity()}){
                for(auto& value:slot.params)value.store(invalid,std::memory_order_release);
                for(int block=0;block<8;++block)processStressBlock(*rack,controls,rate,frameCount);
            }
            for(int change=0;change<18;++change){
                for(int parameter=0;parameter<info.parameterCount;++parameter){
                    const auto& specification=info.params[static_cast<std::size_t>(parameter)];
                    slot.params[static_cast<std::size_t>(parameter)].store((change&1)?specification.maximum:specification.minimum);
                }
                processStressBlock(*rack,controls,rate,frameCount);
            }
        }
        countAllocations=false;
        REQUIRE(allocations==0);
    }
}

void gainChangesAreContinuous() {
    auto rack=std::make_unique<Rack>();
    Controls controls;
    auto& slot=controls.slots[0][0];
    configureSlot(slot,VocalColor,true);
    slot.params[4].store(0); // Pure dry path: output gain has an exact analytic target.
    slot.params[5].store(0);
    rack->prepare(48000,blockSize);
    rack->beginBlock(controls);
    float previous=0;
    for(int sample=0;sample<blockSize;++sample)previous=rack->processSample(0,.1f);
    REQUIRE(std::abs(previous-.1f)<1e-6f);

    countAllocations=true;
    for(float decibels:{12.0f,0.0f,-12.0f,6.0f,0.0f}){
        slot.params[5].store(decibels,std::memory_order_release);
        rack->beginBlock(controls);
        for(int sample=0;sample<64;++sample){
            const float actual=rack->processSample(0,.1f);
            // The worst 0 -> +12 dB change is .298107 / 64 = .004658 per sample.
            REQUIRE(std::isfinite(actual)&&std::abs(actual-previous)<.005f);
            previous=actual;
        }
        const float target=.1f*gain(decibels);
        REQUIRE(std::abs(previous-target)<1e-6f);
        for(int sample=64;sample<blockSize;++sample){
            previous=rack->processSample(0,.1f);
            REQUIRE(std::abs(previous-target)<1e-6f);
        }
    }
    countAllocations=false;
    REQUIRE(allocations==0);
}

void reverbLayoutChangesDiscardOldTail() {
    auto rack=std::make_unique<Rack>();
    Controls controls;
    auto& slot=controls.slots[0][0];
    rack->prepare(48000,blockSize);
    for(int structuralParameter:{3,4}){
        configureSlot(slot,Reverb96,true);
        slot.params[0].store(100); // Entirely wet: a post-change zero is meaningful.
        slot.params[1].store(5);   // Keep a long tail alive before the layout change.
        rack->reset();
        rack->beginBlock(controls);
        for(int sample=0;sample<24000;++sample){
            const float input=static_cast<float>(.18*std::sin(2*pi*311*sample/48000));
            (void)rack->processSample(0,input);
        }
        double oldTailEnergy=0;
        for(int sample=0;sample<512;++sample){
            const float output=rack->processSample(0,0);
            oldTailEnergy+=static_cast<double>(output)*output;
        }
        REQUIRE(oldTailEnergy>1e-5);

        countAllocations=true;
        slot.params[static_cast<std::size_t>(structuralParameter)].store(
            metadata(Reverb96).params[static_cast<std::size_t>(structuralParameter)].maximum);
        for(int block=0;block<300;++block){
            rack->beginBlock(controls);
            for(int sample=0;sample<blockSize;++sample){
                const float output=rack->processSample(0,0);
                REQUIRE(std::isfinite(output));
                // The existing tail may fade for 64 samples; invalidated delay
                // partitions and damping states must never revive it later.
                if(block>0||sample>=64)REQUIRE(output==0.0f);
            }
        }
        countAllocations=false;
        REQUIRE(allocations==0);
    }
}
}

int main(){
    allModulesLiveRanges();
    gainChangesAreContinuous();
    reverbLayoutChangesDiscardOldTail();
    std::cout<<"PASS: "<<lah::builtin::moduleCount
             <<" live native modules; 8-192 kHz, all parameter ranges/invalid values, "
               "channel isolation, zero realtime allocations, 64-sample gain transitions, "
               "and reverb layout changes without stale tails\n";
}
