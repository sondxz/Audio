#include "../src/dsp/MixDspCore.h"
#include "../src/audio/UsbReturnProfile.h"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>

static bool observeAllocations=false;static int allocations=0;
void* operator new(std::size_t n){if(observeAllocations)++allocations;if(auto* p=std::malloc(n))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}void operator delete(void* p,std::size_t)noexcept{std::free(p);}
using namespace lah;
static void close(float value,float expected,float tolerance=1e-5f){assert(std::abs(value-expected)<tolerance);}
int main(){
    dsp::BusMasterControls controls;dsp::BusMasterProcessor processor;std::array<float,32> input{},returns{};input[7]=.1f;
    processor.beginBlock(controls);processor.processSample(input.data(),8,1,returns);processor.endBlock();for(float v:returns)close(v,0);
    controls.enabled.store(true);controls.sends[7*32+7].store(1);controls.output[7].store(7);controls.toMaster[7].store(0);
    processor.beginBlock(controls);processor.processSample(input.data(),8,1,returns);processor.endBlock();close(returns[7],.1f);close(returns[0],0);close(processor.consumePeak(7),.1f);close(processor.consumePeak(0),0);
    controls.gainDb[7].store(-6);processor.beginBlock(controls);processor.processSample(input.data(),8,1,returns);processor.endBlock();close(returns[7],.1f*std::pow(10.0f,-.3f));
    controls.mute[7].store(1);processor.beginBlock(controls);processor.processSample(input.data(),8,1,returns);processor.endBlock();close(returns[7],0);
    dsp::recallBusMaster(controls,{});controls.enabled.store(true);controls.masterSends[7].store(1);controls.inputPan[7].store(0);processor.reset();processor.beginBlock(controls);processor.processSample(input.data(),8,1,returns);processor.endBlock();
    close(returns[0],.1f/std::sqrt(2.0f));close(returns[1],returns[0]);close(returns[0]*returns[0]+returns[1]*returns[1],.01f);
    controls.inputPan[7].store(-1);processor.beginBlock(controls);processor.processSample(input.data(),8,1,returns);processor.endBlock();close(returns[0],.1f);close(returns[1],0);
    controls.masterGainDb[0].store(-12);processor.beginBlock(controls);processor.processSample(input.data(),8,1,returns);processor.endBlock();close(returns[0],.1f*std::pow(10.0f,-.6f));
    controls.masterMute.store(true);processor.beginBlock(controls);processor.processSample(input.data(),8,1,returns);processor.endBlock();close(returns[0],0);close(returns[1],0);
    // Independent buses can sum into stereo Master while retaining their own direct outputs.
    dsp::recallBusMaster(controls,{});controls.enabled.store(true);controls.sends[7].store(1);controls.sends[32+7].store(.5f);controls.pan[0].store(-1);controls.pan[1].store(1);controls.output[0].store(2);
    processor.reset();processor.beginBlock(controls);processor.processSample(input.data(),8,1,returns);processor.endBlock();close(returns[0],.1f);close(returns[1],.05f);close(returns[2],.1f);
    // The per-block gain ramp reaches its target with bounded adjacent samples.
    controls.gainDb[0].store(-12);processor.beginBlock(controls);float previous=.1f;
    for(int s=0;s<128;++s){processor.processSample(input.data(),8,(s+1)/128.0f,returns);assert(std::abs(returns[2]-previous)<.001f);previous=returns[2];}processor.endBlock();close(previous,.1f*std::pow(10.0f,-.6f));
    // Control-thread invalidation rejects even an in-flight meter publication.
    processor.beginBlock(controls);processor.processSample(input.data(),8,1,returns);processor.invalidateMeters();processor.endBlock();close(processor.consumePeak(0),0);
    controls.sends[7].store(std::numeric_limits<float>::quiet_NaN());controls.masterSends[7].store(std::numeric_limits<float>::infinity());controls.output[0].store(500);controls.masterLeft.store(9);controls.masterRight.store(9);
    const auto bounded=dsp::snapshotBusMaster(controls);assert(bounded.output[0]==-1&&bounded.masterRight==-1&&bounded.sends[7]==0&&bounded.masterSends[7]==0);
    allocations=0;observeAllocations=true;for(int n=0;n<10;++n){processor.beginBlock(controls);for(int s=0;s<128;++s)processor.processSample(input.data(),32,(s+1)/128.0f,returns);processor.endBlock();}observeAllocations=false;assert(allocations==0);for(float v:returns)assert(std::isfinite(v));
    // Integration: post-input gain feeds bus -> Master -> physical output gain.
    dsp::Controls mix;for(auto& route:mix.route)route.store(0);mix.inputGain[7].store(-6);mix.outputGain[0].store(-6);mix.busMaster.enabled.store(true);mix.busMaster.sends[7].store(1);mix.busMaster.pan[0].store(-1);
    dsp::Mixer mixer(mix);mixer.prepare(48000);std::array<std::array<float,128>,8> ins{},outs{};std::array<const float*,8> inPointers{};std::array<float*,8> outPointers{};
    for(int i=0;i<8;++i){inPointers[i]=ins[i].data();outPointers[i]=outs[i].data();}ins[7].fill(.1f);
    mixer.process(inPointers.data(),8,outPointers.data(),8,128,true);close(outs[0][127],.1f*std::pow(10.0f,-.6f));for(int o=1;o<8;++o)close(outs[o][127],0);
    mixer.process(inPointers.data(),8,outPointers.data(),8,128,false);for(auto& channel:outs)for(float v:channel)close(v,0);
    audio::applyUsbReturnProfile(mix,8);assert(!mix.busMaster.enabled.load());for(auto& send:mix.busMaster.sends)close(send.load(),0);
    std::cout<<"PASS: 8 buses, stereo constant-power pan, summing, gain/mute, output maps, smoothing, meter epochs, finite bounds, zero allocations, engine integration, USB profile reset\n";
}
