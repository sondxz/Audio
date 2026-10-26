#include "../src/measurement/MeasurementCore.h"
#include "../src/simulation/SubArrayCore.h"
#include "MeasurementTestUtils.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <random>
#include <vector>
#define CHECK(x) do { if(!(x)){ std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << " " << #x << "\n"; std::exit(1);} } while(false)

using lah::measurement::analyze;
using lah::measurement::fftSize;

std::vector<float> noise(std::size_t n,unsigned seed=123){
    std::mt19937 generator(seed); std::normal_distribution<float> d(0.0f,0.16f);
    std::vector<float> r(n);
    for(auto& v:r)v=d(generator);
    return r;
}
std::vector<float> shiftedGain(const std::vector<float>& r,int delay,float gain){
    std::vector<float> y(r.size(),0.0f);
    for(std::size_t i=0;i<r.size();++i){
        const auto source=static_cast<long long>(i)-delay;
        if(source>=0&&source<static_cast<long long>(r.size()))y[i]=gain*r[static_cast<std::size_t>(source)];
    }
    return y;
}
void checkFlatTransfer(int delay,double fs,float gain){
    auto r=noise(32768);
    // Reject invalid timebases before lround/FFT/delay compensation.
    for(double badRate:{std::numeric_limits<double>::quiet_NaN(),
                        std::numeric_limits<double>::infinity(),0.0}) {
        CHECK(!lah::measurement::estimateDelay(r,r,badRate).valid);
        const auto rejectedStorage=lah::test::analyzeHeap(r,r,badRate);
        const auto& rejected=*rejectedStorage;
        CHECK(!rejected.valid && !rejected.hasRta && !rejected.delayValid);
        CHECK(rejected.rtaDb[100]==-160.0);
    }
    CHECK(!lah::measurement::estimateDelay(r,r,48000,
        std::numeric_limits<double>::quiet_NaN()).valid);
    auto measured=shiftedGain(r,delay,gain);
    const auto resultStorage=lah::test::analyzeHeap(r,measured,fs);
    const auto& result=*resultStorage;
    CHECK(result.valid);CHECK(result.delayValid);
    CHECK(result.delaySamples==delay);
    CHECK(result.reliableBins>500);
    for(int k=90;k<1900;k+=29){
        if(!result.binReliable[static_cast<std::size_t>(k)])continue;
        const double target=20*std::log10(std::abs(gain));
        CHECK(std::abs(result.magnitudeDb[static_cast<std::size_t>(k)]-target)<0.08);
        const double phase=std::abs(result.phaseDeg[static_cast<std::size_t>(k)]);
        CHECK(gain>0?phase<1.5:std::abs(phase-180)<1.5);
        CHECK(result.coherence[static_cast<std::size_t>(k)]>0.999);
    }
}

int main(){
    checkFlatTransfer(0,48000,0.5f);
    checkFlatTransfer(120,48000,0.5f);
    checkFlatTransfer(6000,48000,0.5f); // previously outside 8192-sample circular lag range
    checkFlatTransfer(-120,48000,0.5f);
    checkFlatTransfer(70,96000,-1.0f);
    auto r=noise(32768);
    auto none=std::vector<float>(r.size(),0.0f);
    const auto silenceStorage=lah::test::analyzeHeap(none,none,48000);
    const auto& silence=*silenceStorage;
    CHECK(!silence.valid&&!silence.hasRta&&!silence.delayValid);
    const auto noReferenceStorage=lah::test::analyzeHeap(none,r,48000);
    const auto& noReference=*noReferenceStorage;
    CHECK(!noReference.valid&&noReference.hasRta&&!noReference.delayValid);
    auto weak=shiftedGain(r,0,0.000001f);
    const auto weakReferenceStorage=lah::test::analyzeHeap(weak,r,48000);
    const auto& weakReference=*weakReferenceStorage;
    CHECK(!weakReference.valid&&!weakReference.delayValid);
    auto nanReference=r;
    nanReference[7]=std::numeric_limits<float>::quiet_NaN();
    const auto finiteStorage=lah::test::analyzeHeap(nanReference,r,48000);
    const auto& finite=*finiteStorage;
    CHECK(finite.hasRta);CHECK(std::isfinite(finite.meanCoherence));
    // A single-frequency tone is ambiguous as a delay reference. Do not guess timing.
    std::vector<float> tone(r.size()),toneDelayed(r.size());
    for(std::size_t i=0;i<tone.size();++i)tone[i]=0.2f*std::sin(2*lah::measurement::pi*1000*i/48000.0);
    toneDelayed=shiftedGain(tone,40,1);
    auto periodic=lah::measurement::estimateDelay(tone,toneDelayed,48000);
    CHECK(!periodic.valid);
    // Parseval check: window-corrected RTA sum of bin RMS powers approximates time RMS power.
    const auto spectralStorage=lah::test::analyzeHeap(none,tone,48000);
    const auto& spectral=*spectralStorage;
    CHECK(spectral.hasRta&&!spectral.valid);
    double power=0;
    for(double db:spectral.rtaDb)power+=std::pow(10.0,db/10.0);
    CHECK(std::abs(power-0.02)<0.001);
    // Free-field monopole 6dB/octave of distance: 2m vs 1m 6.02dB attenuation.
    std::vector<lah::simulation::Sub> sources{{{0,0,0},0,0,false,true}};
    CHECK(std::abs(lah::simulation::relativeDb(sources,{0,2,0},{343,80})+6.020599913)<1e-4);
    std::cout<<"PASS: linear delay 0, +120, +6000, -120 @48k; inverse @96k; "
             <<"amplitude/phase/coherence gates; silence, absent reference, NaN, periodic ambiguity; RTA Parseval; free-field 1/r\n";
}
