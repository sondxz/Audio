#include "../src/dsp/MixDspCore.h"
#include "../src/measurement/MeasurementCore.h"
#include "../src/simulation/SubArrayCore.h"
#include "MeasurementTestUtils.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <random>
#include <vector>
#define CHECK(x) do { if(!(x)){std::cerr<<"FAIL line "<<__LINE__<<": "<<#x<<'\n';std::exit(1);}} while(false)
using lah::dsp::Controls;
using lah::dsp::Mixer;

void mixerTests() {
    Controls ctl;Mixer m(ctl);m.prepare(48000);
    // Invalid rates must be rejected before conversion to delay-buffer sizes,
    // and a failed reconfiguration must preserve the last working processor.
    for(double invalidRate:{std::numeric_limits<double>::quiet_NaN(),
                            std::numeric_limits<double>::infinity(),
                            -std::numeric_limits<double>::infinity()}) {
        bool mixerRejected=false,effectsRejected=false;
        try {m.prepare(invalidRate);} catch(const std::invalid_argument&) {mixerRejected=true;}
        lah::dsp::StereoEffects effects;
        try {effects.prepare(invalidRate);} catch(const std::invalid_argument&) {effectsRejected=true;}
        CHECK(mixerRejected && effectsRejected);
    }
    float a[512]{},b[512]{},o0[512]{},o1[512]{};
    for(int i=0;i<512;++i){a[i]=0.1f;b[i]=-0.2f;}
    const float* in[]={a,b};float* out[]={o0,o1};float ip[2]{},op[2]{};
    m.process(in,2,out,2,512,true,ip,op,2);
    CHECK(std::abs(o0[400]-0.1f)<1e-4f);
    CHECK(std::abs(o1[400]+0.2f)<1e-4f);
    CHECK(std::abs(ip[0]-0.1f)<1e-4f);
    CHECK(std::abs(op[1]-0.2f)<1e-4f);
    // Mutable matrix routes: channel 0 to output 1, output1 default route removed.
    ctl.route[1*lah::dsp::channels+1].store(0);
    ctl.route[1*lah::dsp::channels+0].store(1);
    m.process(in,2,out,2,512,true);
    CHECK(std::abs(o1[511]-0.1f)<1e-4f);
    // Gain and polarity on output.
    ctl.outputPolarity[0].store(-1);
    ctl.outputGain[0].store(-6.0206f);
    m.process(in,2,out,2,512,true);
    CHECK(std::abs(o0[511]+0.05f)<0.001f);
    // Delay + zero immediately after unmute, no old samples leaking on resume.
    ctl.outputDelayMs[0].store(2);
    m.process(in,2,out,2,512,false);
    CHECK(o0[10]==0 && o1[200]==0);
    m.process(in,2,out,2,512,true);
    CHECK(o0[0]==0.0f && std::abs(o0[450]+0.05f)<0.001f);
    // Default ceiling acts as guard, cannot output > 1.
    ctl.outputDelayMs[0].store(0);
    for(auto& x:a)x=10.0f;
    m.process(in,2,out,2,512,true);
    CHECK(*std::max_element(o1,o1+512)<=1.0f);
    // Stereo delay return: an impulse sent from input 1 is audible again at 20ms.
    Controls fxCtl;Mixer fxMixer(fxCtl);fxMixer.prepare(48000);
    fxCtl.fxSend[0].store(1.0f);
    fxCtl.delayReturn.store(0.5f);
    fxCtl.fxDelayMs.store(20.0f);
    fxCtl.fxFeedback.store(0.0f);
    float impulse[512]{},fxOut0[512]{},fxOut1[512]{};
    impulse[0]=0.1f;
    const float* fxIn[]{impulse};float* fxOut[]{fxOut0,fxOut1};
    fxMixer.process(fxIn,1,fxOut,2,512,true);
    std::fill_n(impulse,512,0.0f);
    fxMixer.process(fxIn,1,fxOut,2,512,true);
    CHECK(std::abs(fxOut0[448]-0.05f)<0.003f); // 960 - 512
    CHECK(std::abs(fxOut1[448])<0.001f);
    // EQ should emphasize a 1000Hz sine above bypass.
    Controls ectl; Mixer em(ectl);em.prepare(48000);ectl.eqDb[0].store(9.0f);
    ectl.eqHz[0].store(1000);ectl.eqQ[0].store(1);
    float sine[512]{};float filtered[512]{};const float* si[]={sine};float* so[]={filtered};
    double aMean=0;
    for(int batch=0;batch<10;++batch){
        for(int i=0;i<512;++i)sine[i]=0.01f*std::sin(2*lah::dsp::pi*1000*(batch*512+i)/48000.0);
        em.process(si,1,so,1,512,true);
        if(batch>5)for(auto x:filtered)aMean+=x*x;
    }
    CHECK(aMean>0.01); // significantly more energy than bypass (~0.1 energy for 2048 samples)
    // Stability on NaN/Inf, zero samples, unavailable channels and 192k prepare.
    em.prepare(192000);sine[0]=std::numeric_limits<float>::infinity();sine[1]=std::numeric_limits<float>::quiet_NaN();
    em.process(si,1,so,1,512,true);for(float x:filtered)CHECK(std::isfinite(x));
    em.process(nullptr,0,so,1,0,true);
}
void fullMatrixTests() {
    for(double sampleRate:{44100.0,48000.0,96000.0,192000.0}) {
        Controls c; Mixer mixer(c);mixer.prepare(sampleRate);
        for(int output=0;output<32;++output)
            for(int input=0;input<32;++input)
                c.route[output*32+input].store(0.25f);
        std::array<std::array<float,256>,32> sources{},sinks{};
        std::array<const float*,32> inputs{};
        std::array<float*,32> outputs{};
        float expected=0;
        for(int i=0;i<32;++i) {
            for(auto& sample:sources[static_cast<std::size_t>(i)])sample=0.001f*static_cast<float>(i+1);
            expected+=0.25f*0.001f*static_cast<float>(i+1);
            inputs[static_cast<std::size_t>(i)]=sources[static_cast<std::size_t>(i)].data();
            outputs[static_cast<std::size_t>(i)]=sinks[static_cast<std::size_t>(i)].data();
        }
        mixer.process(inputs.data(),32,outputs.data(),32,256,true);
        for(auto& ch:sinks)for(const auto value:ch)CHECK(std::abs(value-expected)<1e-4f);
        mixer.process(inputs.data(),32,outputs.data(),32,256,false);
        for(auto& ch:sinks)for(const auto value:ch)CHECK(value==0.0f);
    }
}
void measurementTests() {
    using namespace lah::measurement;
    auto capture=std::make_unique<Capture>();
    auto& c=*capture;
    std::vector<float> r(32768),m(32768);
    std::mt19937 gen(1); std::uniform_real_distribution<float> dist(-0.5f,0.5f);
    for(int i=0;i<32768;++i){r[i]=dist(gen);m[i]=0.5f*r[i];}
    c.put(r.data(),m.data(),32768);std::vector<float> snapr,snapm;
    CHECK(c.snapshot(snapr,snapm,32768));
    const auto resultStorage=lah::test::analyzeHeap(snapr,snapm,48000);
    const auto& result=*resultStorage;
    CHECK(result.valid);CHECK(result.segments>=4);
    CHECK(result.meanCoherence>0.999);
    CHECK(result.delayValid && result.delaySamples==0);
    for(int i=80;i<3500;++i){
        CHECK(std::abs(result.magnitudeDb[i]+6.0206)<0.03);
        CHECK(std::abs(result.phaseDeg[i])<0.01);
    }
    CHECK(!lah::test::analyzeHeap({}, {},48000)->valid);
    c.put(nullptr,m.data(),32768);
    CHECK(c.snapshot(snapr,snapm,32768));
    const auto rtaStorage=lah::test::analyzeHeap(snapr,snapm,48000);
    const auto& rtaOnly=*rtaStorage;
    CHECK(rtaOnly.hasRta && !rtaOnly.valid);
    std::vector<float> shifted(32768,0.0f);
    for(std::size_t i=120;i<shifted.size();++i)shifted[i]=r[i-120];
    const auto delayedStorage=lah::test::analyzeHeap(r,shifted,48000);
    const auto& delayed=*delayedStorage;
    CHECK(delayed.valid && delayed.delayValid);
    std::vector<float> inverted(32768);
    for(std::size_t i=0;i<inverted.size();++i) inverted[i]=-r[i];
    const auto invertedStorage=lah::test::analyzeHeap(r,inverted,48000);
    const auto& invertedMeasurement=*invertedStorage;
    CHECK(invertedMeasurement.valid && invertedMeasurement.delayValid);
    CHECK(std::abs(std::abs(invertedMeasurement.phaseDeg[400])-180)<1.0);
    CHECK(std::abs(delayed.delaySamples-120)<=1);
}
void subTests(){
    using namespace lah::simulation;
    std::vector<Sub> single{{{0,0,0},0,0,false,true}};
    CHECK(std::abs(relativeDb(single,{0,1,0},{343,80}))<1e-5);
    CHECK(std::abs(relativeDb(single,{0,2,0},{343,80})+6.0206)<0.0001);
    auto two=single;two.push_back(single[0]);
    CHECK(std::abs(relativeDb(two,{0,1,0},{343,80})-6.0206)<0.0001);
    two[1].invert=true;
    CHECK(relativeDb(two,{0,1,0},{343,80}) < -90);
    auto endfire=preset(2,343.0/(4*80));
    CHECK(relativeDb(endfire,{0,25,1.2},{343,80}) > relativeDb(endfire,{0,-25,1.2},{343,80}));
    auto grid=simulate2D(endfire,{343,80},32,20,30,30);
    CHECK(grid.db.size()==640);CHECK(grid.maxDb>grid.minDb);
    for(auto v:grid.db)CHECK(std::isfinite(v));
}
int main(){mixerTests();fullMatrixTests();measurementTests();subTests();std::cout<<"PASS: audio mixer, DSP, 32x32 routing, delay/polarity/mute, transfer FFT/coherence, 3D free-field pressure\n";}
