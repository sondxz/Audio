#pragma once
// Offline sine/phasor teaching model. Never connected to the audio callback.
// Convention: y(t)=A*sin(2*pi*f*t + phase - 2*pi*f*delay).
// A positive delay RETARDS the signal; a positive phase setting LEADS it.
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace lah::engineering::phase {
constexpr double pi=3.141592653589793238462643383279502884;
constexpr std::size_t maximumSources=16;
constexpr double minimumFrequencyHz=0.1,maximumFrequencyHz=20000.0;

inline void require(double value,double low,double high,const char* message){
    if(!std::isfinite(value) || value<low || value>high)throw std::invalid_argument(message);
}
inline void checkFrequency(double frequencyHz){
    require(frequencyHz,minimumFrequencyHz,maximumFrequencyHz,"Frequency must be 0.1..20000 Hz");
}
// Canonical interval [-180,180). Full cycles are intentionally discarded here.
inline double wrapDegrees(double degrees){
    require(degrees,-1.0e12,1.0e12,"Phase must be finite and within +/-1e12 degrees");
    auto wrapped=std::fmod(degrees,360.0);
    if(wrapped>=180.0)wrapped-=360.0;
    if(wrapped< -180.0)wrapped+=360.0;
    return wrapped==0.0?0.0:wrapped;
}
inline double periodMs(double frequencyHz){checkFrequency(frequencyHz);return 1000.0/frequencyHz;}
inline double delayPhaseDegrees(double frequencyHz,double delayMs){
    checkFrequency(frequencyHz);require(delayMs,-10000.0,10000.0,"Delay must be +/-10000 ms");
    return -0.36*frequencyHz*delayMs;
}
// phase + 360*k is the chosen UNWRAPPED branch. Wrapped phase alone cannot
// determine a unique delay; k must be supplied by the user/reference context.
inline double phaseToDelayMs(double phaseDegrees,double frequencyHz,int cycleBranch=0){
    checkFrequency(frequencyHz);require(phaseDegrees,-1.0e9,1.0e9,"Phase conversion out of bounds");
    if(cycleBranch< -10000 || cycleBranch>10000)throw std::invalid_argument("Cycle branch out of bounds");
    return -(phaseDegrees+360.0*cycleBranch)/(0.36*frequencyHz);
}
inline double phaseDelayToFrequencyHz(double phaseDegrees,double delayMs,int cycleBranch=0){
    require(delayMs,-10000.0,10000.0,"Delay must be +/-10000 ms");
    require(phaseDegrees,-1.0e9,1.0e9,"Phase conversion out of bounds");
    if(cycleBranch< -10000 || cycleBranch>10000)throw std::invalid_argument("Cycle branch out of bounds");
    if(delayMs==0.0)throw std::invalid_argument("Zero delay does not determine a unique frequency");
    const auto frequency=-(phaseDegrees+360.0*cycleBranch)/(0.36*delayMs);
    checkFrequency(frequency);return frequency;
}

struct Source {
    double frequencyHz=100.0,phaseDegrees=0.0,delayMs=0.0,levelDb=0.0;
    bool inverted=false,enabled=true;
};
inline void validate(const Source& source){
    checkFrequency(source.frequencyHz);
    require(source.phaseDegrees,-360000.0,360000.0,"Source phase must be +/-360000 degrees");
    require(source.delayMs,-10000.0,10000.0,"Source delay must be +/-10000 ms");
    require(source.levelDb,-120.0,24.0,"Relative level must be -120..+24 dB");
}
inline void validate(const std::vector<Source>& sources){
    if(sources.size()<2 || sources.size()>maximumSources)
        throw std::invalid_argument("Sine model requires 2..16 sources");
    for(const auto& source:sources)validate(source);
}
inline double amplitude(const Source& source){validate(source);return source.enabled?std::pow(10.0,source.levelDb/20.0):0.0;}
inline double effectivePhaseDegrees(const Source& source){
    validate(source);
    return source.phaseDegrees+delayPhaseDegrees(source.frequencyHz,source.delayMs)+(source.inverted?180.0:0.0);
}
inline std::complex<double> phasor(const Source& source,double timeSeconds=0.0){
    require(timeSeconds,-60.0,60.0,"Display time must be +/-60 seconds");
    const auto degrees=wrapDegrees(effectivePhaseDegrees(source)+360.0*source.frequencyHz*timeSeconds);
    return std::polar(amplitude(source),degrees*pi/180.0);
}
inline double sample(const Source& source,double timeSeconds){return phasor(source,timeSeconds).imag();}

struct Sum {
    std::complex<double> instantaneousPhasor{};
    double instantaneousSample=0.0,upperPeakBound=0.0,steadyPeak=0.0,steadyRms=0.0;
    double referencePhaseDegrees=0.0;
    std::size_t activeSources=0;
    // steadyPeak/Rms/referencePhase are meaningful only for a common frequency.
    bool commonFrequency=false,phaseDefined=false;
};
inline Sum combine(const std::vector<Source>& sources,double timeSeconds=0.0){
    validate(sources);require(timeSeconds,-60.0,60.0,"Display time must be +/-60 seconds");
    Sum result;
    std::complex<double> reference{};
    double frequency=0.0;
    bool matched=true;
    for(const auto& source:sources){
        if(!source.enabled)continue;
        if(result.activeSources==0)frequency=source.frequencyHz;
        else if(source.frequencyHz!=frequency)matched=false;
        ++result.activeSources;
        result.upperPeakBound+=amplitude(source);
        reference+=phasor(source);
        result.instantaneousPhasor+=phasor(source,timeSeconds);
    }
    result.instantaneousSample=result.instantaneousPhasor.imag();
    result.commonFrequency=result.activeSources>0 && matched;
    if(result.commonFrequency){
        result.steadyPeak=std::abs(reference);
        result.steadyRms=result.steadyPeak/std::sqrt(2.0);
        // Numerically cancelled sums have no useful phase. Preserve residual
        // amplitude, but do not publish an arbitrary atan2 phase at a null.
        result.phaseDefined=result.steadyPeak>result.upperPeakBound*1.0e-12;
        if(result.phaseDefined)result.referencePhaseDegrees=wrapDegrees(std::arg(reference)*180.0/pi);
    }
    return result;
}

struct Waveform {
    double startSeconds=0.0,durationSeconds=0.0,upperPeakBound=0.0;
    std::vector<std::vector<double>> sources;
    std::vector<double> sum;
    bool adequateTemporalResolution=false;
};
inline Waveform waveform(const std::vector<Source>& sources,double startSeconds,double durationSeconds,
                         std::size_t points=1025){
    validate(sources);require(startSeconds,-60.0,60.0,"Waveform start out of bounds");
    require(durationSeconds,1.0e-9,60.0,"Waveform duration out of bounds");
    require(startSeconds+durationSeconds,-60.0,60.0,"Waveform end out of bounds");
    if(points<33 || points>8193)throw std::invalid_argument("Waveform requires 33..8193 display points");
    Waveform result;result.startSeconds=startSeconds;result.durationSeconds=durationSeconds;
    result.sources.resize(sources.size());result.sum.assign(points,0.0);
    double highestFrequency=0.0;
    for(std::size_t index=0;index<sources.size();++index){
        const auto& source=sources[index];
        const auto gain=amplitude(source),offset=effectivePhaseDegrees(source);
        result.upperPeakBound+=gain;
        if(source.enabled)highestFrequency=std::max(highestFrequency,source.frequencyHz);
        auto& trace=result.sources[index];trace.resize(points);
        for(std::size_t point=0;point<points;++point){
            const double time=startSeconds+durationSeconds*static_cast<double>(point)/static_cast<double>(points-1);
            const auto degrees=wrapDegrees(offset+360.0*source.frequencyHz*time);
            trace[point]=gain*std::sin(degrees*pi/180.0);
            result.sum[point]+=trace[point];
        }
    }
    // A drawing is withheld by the UI when there are fewer than 16 samples per
    // highest-frequency cycle; a sparse connected line would be misleading.
    result.adequateTemporalResolution=highestFrequency*durationSeconds<=static_cast<double>(points-1)/16.0;
    return result;
}
} // namespace lah::engineering::phase
