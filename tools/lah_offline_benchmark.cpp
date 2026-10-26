// Synthetic offline stress only. This does NOT measure ASIO round-trip latency,
// realtime scheduling jitter, third-party plugin behavior or any device xrun.
#include "../src/dsp/MixDspCore.h"
#include "../src/fir/FIRCore.h"
#include "../src/audio/LatencyCompensator.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <numeric>
#include <vector>
int main(){
    constexpr int frames=128,channelCount=32,iterations=1000;
    lah::dsp::Controls controls;
    for(auto& r:controls.route)r.store(0.02f);
    lah::dsp::Mixer mixer(controls);mixer.prepare(48000);
    auto design=lah::fir::make(lah::fir::Mode::Lowpass,1500,48000,257);
    lah::fir::StereoRealtimeFIR fir;fir.prepare(design,48000);
    lah::audio::LatencyCompensator alignment;
    alignment.prepare();alignment.configure(fir.latencySamples());
    std::array<std::array<float,frames>,channelCount> sources{},destinations{};
    std::array<const float*,channelCount> in{};
    std::array<float*,channelCount> out{};
    for(int ch=0;ch<channelCount;++ch){
        in[ch]=sources[ch].data();out[ch]=destinations[ch].data();
        for(int s=0;s<frames;++s)sources[ch][s]=static_cast<float>(0.01*std::sin((ch+1)*0.01*s));
    }
    std::vector<double> microseconds;microseconds.reserve(iterations);
    for(int iter=0;iter<iterations+100;++iter){
        const auto start=std::chrono::steady_clock::now();
        mixer.process(in.data(),channelCount,out.data(),channelCount,frames,true);
        fir.process(out[0],out[1],frames);
        alignment.process(out.data(),channelCount,frames);
        const auto time=std::chrono::steady_clock::now()-start;
        if(iter>=100)microseconds.push_back(std::chrono::duration<double,std::micro>(time).count());
    }
    std::sort(microseconds.begin(),microseconds.end());
    const double maxBudgetUs=1e6*frames/48000.0;
    std::cout<<"OFFLINE SYNTHETIC BENCHMARK — not ASIO realtime evidence\n"
             <<"32x32 dense mixer + 2ch 257-tap FIR + OUT3..32 static alignment, 48 kHz, 128 frames\n"
             <<"p50="<<microseconds[iterations/2]<<" us, p99="<<microseconds[iterations*99/100]
             <<" us, max="<<microseconds.back()<<" us; callback period="<<maxBudgetUs
             <<" us (EXCLUDES driver/OS, UI and VST3)\n";
}
