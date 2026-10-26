#include "../src/reliability/StreamSafety.h"
#include "../src/measurement/MeasurementCore.h"
#include "../src/measurement/ImpulseCore.h"
#include "../src/fir/FIRCore.h"
#include "../src/spl/CalibratedSplCore.h"
#include "../src/dsp/MixDspCore.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <random>
#include <vector>

int main() {
    using namespace lah;
    // Callback deadline lock stays safe until an OPERATOR resets it.
    reliability::StreamSafety guard;
    for (int i=0;i<7;++i) assert(guard.record(128,48000,3000000));
    assert(!guard.record(128,48000,3000000));
    assert(guard.isFaulted());
    assert(!guard.record(128,48000,10000));
    guard.clearWhileMuted();
    assert(!guard.isFaulted());
    for(int i=0;i<160;++i) {
        assert(guard.record(64,48000,1000000));
        assert(guard.record(64,48000,1350000)); // sporadic overrun must NOT trigger latch
    }
    assert(!guard.isFaulted());
    assert(!guard.record(0,48000,100));
    guard.clearWhileMuted();
    // Synthetic dual-channel reference with precisely known transport delay and gain.
    constexpr int N=32768, delay=173;
    std::mt19937 gen(0xCAFE4321);
    std::uniform_real_distribution<float> distribution(-0.5f,0.5f);
    std::vector<float> reference(N), response(N,0.0f);
    for(auto& f:reference) f=distribution(gen);
    for(int i=delay;i<N;++i) response[i]=reference[i-delay]*0.25f;
    const auto found=measurement::estimateDelay(reference,response,48000);
    assert(found.valid && found.samples==delay);
    const auto tf=measurement::analyze(reference,response,48000);
    assert(tf.valid && tf.delayValid && tf.delaySamples==delay);
    const auto ir=measurement::deriveRelativeImpulse(tf);
    assert(ir.valid && ir.usedBins>1000 && ir.peak>0.20 && ir.peak<0.30);
    // 32k broadband bins, crop out low-frequency / Nyquist edge bins.
    int accepted=0;
    for(int k=80;k<3000;k+=53) {
        if(tf.binReliable[static_cast<std::size_t>(k)]) {
            assert(std::abs(tf.magnitudeDb[static_cast<std::size_t>(k)]+12.0411998266)<0.15);
            assert(std::abs(tf.phaseDeg[static_cast<std::size_t>(k)])<1.0);
            ++accepted;
        }
    }
    assert(accepted>40);
    // Coherent 1kHz calibration, actual A/C/Z only compared for synthetic tone.
    std::vector<float> calibration(32768);
    for (std::size_t i=0;i<calibration.size();++i)
        calibration[i]=0.1f*std::sin(2*measurement::pi*1000*i/48000.0);
    const auto cal=spl::calibrate1000Hz(calibration,48000,94.0);
    assert(cal.valid);
    const auto reading=spl::analyze(calibration,48000,cal);
    assert(reading.valid);
    assert(std::abs(reading.dbZ-94)<0.05);
    assert(std::abs(reading.dbA-94)<0.1);
    assert(std::abs(reading.dbC-94)<0.1);
    // All FIR taps and actual direct-form processing must reproduce convolution.
    const auto design=fir::make(fir::Mode::Lowpass,3000,48000,257);
    fir::StereoRealtimeFIR realtime;
    assert(realtime.prepare(design,48000));
    assert(!realtime.prepare(design,44100)); // clock mismatch MUST fail, not resample implicitly
    assert(realtime.prepare(design,48000));
    fir::Convolver referenceFir(design);
    std::array<float,1000> left{},right{};
    left[0]=1;right[0]=1;
    realtime.process(left.data(),right.data(),static_cast<int>(left.size()));
    for(std::size_t i=0;i<left.size();++i) {
        const double expected=referenceFir.tick(i==0?1.0:0.0);
        assert(std::abs(left[i]-expected)<1e-6);
        assert(std::abs(right[i]-expected)<1e-6);
    }
    assert(realtime.latencySamples()==128);
    // Route + mute + panic regression on dry audio with no hardware.
    dsp::Controls controls;
    dsp::Mixer mixer(controls);
    mixer.prepare(48000);
    constexpr int frames=128;
    float input[frames],outL[frames],outR[frames];
    std::fill_n(input,frames,0.25f);
    const float* in[]={input};float* out[]={outL,outR};
    controls.route[32].store(1.0f);
    mixer.process(in,1,out,2,frames,true);
    assert(outL[40]>0.20 && outR[40]>0.20);
    mixer.process(in,1,out,2,frames,false);
    assert(std::all_of(outL,outL+frames,[](float v){return v==0;}));
    assert(std::all_of(outR,outR+frames,[](float v){return v==0;}));
    std::cout << "operational acceptance (synthetic): safety, TF, delay, IR, SPL, FIR, routing/PANIC passed\n";
}
