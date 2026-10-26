#include "../src/engineering/PhaseCore.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

#define CHECK(condition) do {if(!(condition)){std::cerr<<"FAIL line "<<__LINE__<<": "<<#condition<<'\n';std::exit(1);}}while(false)
static bool near(double left,double right,double tolerance=1.0e-10){return std::abs(left-right)<=tolerance;}
template<class Function> static void rejects(Function function){
    bool rejected=false;try{function();}catch(const std::invalid_argument&){rejected=true;}
    CHECK(rejected);
}
int main(){
    using namespace lah::engineering::phase;
    // Independent quarter-cycle facts: a 100 Hz period is 10 ms, so a positive
    // 2.5 ms delay retards a sine by 90 degrees (not +90 degrees).
    CHECK(near(periodMs(100.0),10.0));
    CHECK(near(delayPhaseDegrees(100.0,2.5),-90.0));
    CHECK(near(delayPhaseDegrees(100.0,-2.5),90.0));
    // Existing engineering example, with the signal-phase sign made explicit:
    // 85 Hz * 2 ms is 0.17 cycle = 61.2 degrees of lag, or -61.2 signal phase.
    CHECK(near(delayPhaseDegrees(85.0,2.0),-61.2));
    CHECK(near(delayPhaseDegrees(100.0,10.0),-360.0));
    CHECK(near(wrapDegrees(-360.0),0.0));
    CHECK(near(wrapDegrees(180.0),-180.0));
    CHECK(near(wrapDegrees(-540.0),-180.0));
    CHECK(near(wrapDegrees(1080.0+45.0),45.0));
    CHECK(near(phaseToDelayMs(90.0,100.0),-2.5));
    CHECK(near(phaseToDelayMs(-90.0,100.0),2.5));
    CHECK(near(phaseToDelayMs(-90.0,100.0,-1),12.5));
    CHECK(near(phaseToDelayMs(90.0,100.0,1),-12.5));
    CHECK(near(phaseDelayToFrequencyHz(-90.0,2.5),100.0));
    CHECK(near(phaseDelayToFrequencyHz(90.0,-2.5),100.0));
    CHECK(near(phaseDelayToFrequencyHz(0.0,10.0,-1),100.0));
    for(double frequency:{0.1,20.0,85.0,1000.0,20000.0})for(double delay:{-10000.0,-2.5,0.0,2.5,10000.0})
        CHECK(near(phaseToDelayMs(delayPhaseDegrees(frequency,delay),frequency),delay,1.0e-8));

    std::vector<Source> sources(2);
    auto result=combine(sources);
    CHECK(result.commonFrequency && result.phaseDefined && result.activeSources==2);
    CHECK(near(result.steadyPeak,2.0));
    CHECK(near(result.steadyRms,std::sqrt(2.0)));
    CHECK(near(20.0*std::log10(result.steadyPeak),6.020599913279624));
    CHECK(near(result.referencePhaseDegrees,0.0));
    sources[1].phaseDegrees=180.0;result=combine(sources);
    CHECK(result.steadyPeak<1.0e-12 && !result.phaseDefined);
    sources[1].phaseDegrees=90.0;result=combine(sources);
    // 1 + j is a known Cartesian phasor, not another implementation of the
    // same summation loop. Its peak is sqrt(2), RMS is 1, phase is +45 deg.
    CHECK(near(result.instantaneousPhasor.real(),1.0));
    CHECK(near(result.instantaneousPhasor.imag(),1.0));
    CHECK(near(result.steadyPeak,std::sqrt(2.0)) && near(result.steadyRms,1.0));
    CHECK(near(result.referencePhaseDegrees,45.0));
    CHECK(near(result.instantaneousSample,1.0));
    sources[1].phaseDegrees=0.0;sources[1].delayMs=2.5;result=combine(sources);
    CHECK(near(result.instantaneousPhasor.real(),1.0));
    CHECK(near(result.instantaneousPhasor.imag(),-1.0));
    CHECK(near(result.referencePhaseDegrees,-45.0));
    // Explicit polarity inversion is equivalent to 180-degree offset at all
    // sampled times, including a nonzero signed delay and phase setting.
    Source inverted{337.0,37.0,-1.25,-7.0,true,true};
    Source rotated=inverted;rotated.inverted=false;rotated.phaseDegrees+=180.0;
    for(double time:{-0.01,0.0,0.00123,0.03})CHECK(near(sample(inverted,time),sample(rotated,time),1.0e-11));
    sources.assign(2,{});sources[1].phaseDegrees=180.0;sources[1].levelDb=20.0*std::log10(0.5);
    result=combine(sources);CHECK(near(result.steadyPeak,0.5));CHECK(near(result.referencePhaseDegrees,0.0));
    sources.assign(3,{});sources[1].phaseDegrees=120.0;sources[2].phaseDegrees=240.0;
    result=combine(sources);CHECK(result.steadyPeak<1.0e-12 && !result.phaseDefined);
    sources.assign(4,{});for(std::size_t index=0;index<sources.size();++index)sources[index].phaseDegrees=90.0*static_cast<double>(index);
    result=combine(sources);CHECK(result.steadyPeak<1.0e-12 && !result.phaseDefined);
    sources.assign(maximumSources,{});result=combine(sources);CHECK(near(result.steadyPeak,16.0));
    for(auto& source:sources)source.enabled=false;
    result=combine(sources);CHECK(result.activeSources==0 && !result.commonFrequency && !result.phaseDefined);
    CHECK(result.instantaneousSample==0.0 && result.upperPeakBound==0.0);

    sources.assign(2,{});sources[1].frequencyHz=200.0;result=combine(sources,0.0025);
    CHECK(!result.commonFrequency && !result.phaseDefined);
    CHECK(near(result.instantaneousPhasor.real(),-1.0));
    CHECK(near(result.instantaneousPhasor.imag(),1.0));
    CHECK(near(result.instantaneousSample,1.0));
    CHECK(result.steadyPeak==0.0 && result.steadyRms==0.0); // Undefined, indicated by commonFrequency=false.

    // Independent continuous-time energy: two identical unit sines sum to
    // 2*sin(theta), with mean-square 2 over any complete number of periods.
    sources.assign(2,{});
    const auto wave=waveform(sources,0.0,0.02,1025);
    CHECK(wave.adequateTemporalResolution && wave.sum.size()==1025 && wave.sources.size()==2);
    CHECK(near(wave.sum[128],2.0));
    CHECK(near(wave.sum[384],-2.0));
    double energy=0;
    for(std::size_t index=0;index<wave.sum.size();++index){
        const double weight=(index==0 || index+1==wave.sum.size())?0.5:1.0;
        energy+=weight*wave.sum[index]*wave.sum[index];
        CHECK(std::isfinite(wave.sum[index]));
        CHECK(near(wave.sum[index],wave.sources[0][index]+wave.sources[1][index]));
    }
    CHECK(near(energy/static_cast<double>(wave.sum.size()-1),2.0,1.0e-10));
    sources[1].frequencyHz=20000.0;
    CHECK(!waveform(sources,0.0,1.0,1025).adequateTemporalResolution);
    sources[1].enabled=false;
    CHECK(waveform(sources,0.0,0.02,1025).adequateTemporalResolution);
    CHECK(near(combine(sources).steadyPeak,1.0));

    const auto nan=std::numeric_limits<double>::quiet_NaN(),inf=std::numeric_limits<double>::infinity();
    rejects([&]{(void)periodMs(0);});rejects([&]{(void)periodMs(inf);});
    rejects([&]{(void)periodMs(0.099);});rejects([&]{(void)periodMs(20000.001);});
    rejects([&]{(void)wrapDegrees(1.0e12+1.0);});
    rejects([&]{(void)wrapDegrees(nan);});rejects([&]{(void)delayPhaseDegrees(100,nan);});
    rejects([&]{(void)phaseToDelayMs(inf,100);});rejects([&]{(void)phaseToDelayMs(90,100,10001);});
    rejects([&]{(void)phaseDelayToFrequencyHz(0,0);});rejects([&]{(void)phaseDelayToFrequencyHz(90,2.5);});
    rejects([&]{(void)phaseDelayToFrequencyHz(-90,inf);});
    for(int parameter=0;parameter<4;++parameter){
        Source source;
        if(parameter==0)source.frequencyHz=nan;else if(parameter==1)source.phaseDegrees=inf;
        else if(parameter==2)source.delayMs=nan;else source.levelDb=inf;
        rejects([&]{(void)phasor(source);});
    }
    rejects([&]{(void)combine(std::vector<Source>(1));});
    rejects([&]{(void)combine(std::vector<Source>{});});
    rejects([&]{(void)combine(std::vector<Source>(maximumSources+1));});
    rejects([&]{(void)combine(sources,inf);});
    rejects([&]{(void)waveform(sources,0,0);});rejects([&]{(void)waveform(sources,59,2);});
    rejects([&]{(void)waveform(sources,0,1,32);});rejects([&]{(void)waveform(sources,0,1,8194);});
    Source boundary{20000.0,360000.0,10000.0,24.0,true,true};
    CHECK(std::isfinite(sample(boundary,60.0)));
    boundary.phaseDegrees=360000.001;rejects([&]{(void)sample(boundary,0);});boundary.phaseDegrees=0;
    boundary.delayMs=-10000.001;rejects([&]{(void)sample(boundary,0);});boundary.delayMs=0;
    boundary.levelDb=24.001;rejects([&]{(void)sample(boundary,0);});
    std::cout<<"PASS: Phase Engine known phasors, cancellation, delay sign, conversion branches, mixed-frequency guards, waveform energy and invalid inputs\n";
}
