#pragma once
#include <array>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <limits>
#include <cstdint>

namespace lah::dynamicspro {
enum Parameter {GateOn,GateThreshold,GateRange,GateRatio,GateAttack,GateHold,GateRelease,
    CompOn,CompThreshold,CompRatio,CompAttack,CompRelease,CompKnee,CompMakeup,
    LimiterOn,LimiterCeiling,LimiterRelease,Wet,Output,parameterCount};
struct ParameterSpec {const char* name;const char* unit;float minimum,maximum,step,defaultValue;};
inline const std::array<ParameterSpec,parameterCount>& parameterSpecs()noexcept {
    static const std::array<ParameterSpec,parameterCount> specs{{
        {"Gate / expander","",0,1,1,1},{"Threshold","dBFS",-100,0,.1f,-70},{"Maximum attenuation","dB",0,100,.1f,60},
        {"Expansion ratio",":1",1,20,.1f,4},{"Opening attack","ms",.1f,100,.1f,2},{"Hold","ms",0,500,1,50},{"Closing release","ms",5,2000,1,150},
        {"Compressor","",0,1,1,1},{"Threshold","dBFS",-60,0,.1f,-18},{"Ratio",":1",1,20,.1f,3},
        {"Attack","ms",.1f,100,.1f,10},{"Release","ms",10,2500,1,120},{"Soft knee","dB",0,24,.1f,6},{"Makeup","dB",-12,24,.1f,0},
        {"Final sample peak limiter","",0,1,1,1},{"Ceiling","dBFS",-24,0,.1f,-1},{"Release","ms",1,1000,1,80},
        {"Processed mix","%",0,100,1,100},{"Output trim","dB",-24,12,.1f,0}
    }};return specs;
}
using Values=std::array<float,parameterCount>;
inline Values defaults()noexcept {Values result{};for(std::size_t i=0;i<result.size();++i)result[i]=parameterSpecs()[i].defaultValue;return result;}
struct Snapshot {bool enabled=false;Values params=defaults();};
struct GainReduction {float gateDb=0,compressorDb=0,limiterDb=0;std::uint64_t generation=0;};
inline bool operator==(const Snapshot& a,const Snapshot& b)noexcept{return a.enabled==b.enabled && a.params==b.params;}
inline bool operator!=(const Snapshot& a,const Snapshot& b)noexcept{return !(a==b);}
inline Snapshot bounded(Snapshot value)noexcept {
    const auto& specs=parameterSpecs();for(std::size_t i=0;i<value.params.size();++i){auto& p=value.params[i];p=std::isfinite(p)?std::clamp(p,specs[i].minimum,specs[i].maximum):specs[i].defaultValue;}
    for(auto index:{GateOn,CompOn,LimiterOn})value.params[static_cast<std::size_t>(index)]=value.params[static_cast<std::size_t>(index)]>=.5f?1.f:0.f;
    return value;
}
struct Controls {
    std::atomic<bool> enabled{false};std::array<std::atomic<float>,parameterCount> params{};
    Controls()noexcept{const auto values=defaults();for(std::size_t i=0;i<values.size();++i)params[i].store(values[i],std::memory_order_relaxed);}
};
inline Snapshot snapshot(const Controls& source)noexcept {Snapshot value;value.enabled=source.enabled.load(std::memory_order_acquire);for(std::size_t i=0;i<value.params.size();++i)value.params[i]=source.params[i].load(std::memory_order_relaxed);return bounded(value);}
inline void recall(Controls& target,Snapshot value)noexcept {value=bounded(value);for(std::size_t i=0;i<value.params.size();++i)target.params[i].store(value.params[i],std::memory_order_relaxed);target.enabled.store(value.enabled,std::memory_order_release);}
inline void reset(Controls& target)noexcept {recall(target,Snapshot{});}

// Positive attenuation in dB. These static curves are also used by the editor;
// they describe settled detector levels, not a measured spectrum or waveform.
inline double gateCurveAttenuationDb(double inputDb,const Snapshot& settings)noexcept {
    const auto& p=settings.params;if(p[GateOn]<.5f)return 0;
    return std::clamp((static_cast<double>(p[GateThreshold])-inputDb)*(p[GateRatio]-1),0.,static_cast<double>(p[GateRange]));
}
inline double compressorCurveReductionDb(double inputDb,const Snapshot& settings)noexcept {
    const auto& p=settings.params;if(p[CompOn]<.5f)return 0;
    const double over=inputDb-p[CompThreshold],knee=p[CompKnee],slope=1.-1./p[CompRatio];
    if(knee<=0)return std::max(0.,over*slope);
    if(over<=-knee*.5)return 0;
    if(over>=knee*.5)return over*slope;
    const double distance=over+knee*.5;return slope*distance*distance/(2*knee);
}
class Processor final {
public:
    void configure(Snapshot value,double rate)noexcept {
        value=bounded(value);const double nextRate=std::isfinite(rate)&&rate>=8000 && rate<=192000?rate:48000;
        if(nextRate!=sampleRate || value.enabled!=settings.enabled)reset();
        if(value.params[GateOn]!=settings.params[GateOn]){gateReduction=gateDetector=0;holdSamples=0;}
        if(value.params[CompOn]!=settings.params[CompOn])compReduction=0;
        if(value.params[LimiterOn]!=settings.params[LimiterOn])limiterGain=1;
        settings=value;sampleRate=nextRate;const auto& p=settings.params;
        gateAttack=coefficient(p[GateAttack]);gateRelease=coefficient(p[GateRelease]);gateDetectorRelease=coefficient(1);
        compAttack=coefficient(p[CompAttack]);compRelease=coefficient(p[CompRelease]);limiterRelease=coefficient(p[LimiterRelease]);
        holdDuration=static_cast<int>(std::lround(p[GateHold]*.001*sampleRate));
        makeup=std::pow(10.,p[CompMakeup]/20.);outputGain=std::pow(10.,p[Output]/20.);ceiling=std::pow(10.,p[LimiterCeiling]/20.);wet=p[Wet]*.01;
    }
    void reset()noexcept {gateReduction=compReduction=gateDetector=0;limiterGain=1;holdSamples=0;}
    float tick(float input)noexcept {
        const double dry=std::isfinite(input)?input:0;
        if(!settings.enabled){reset();return static_cast<float>(dry);}
        const auto& p=settings.params;double x=dry;
        if(p[GateOn]>=.5f){
            gateDetector=std::max(std::abs(x),gateDetector*gateDetectorRelease);
            const double level=toDb(gateDetector);double target=0;
            if(level>=p[GateThreshold])holdSamples=holdDuration;
            else if(holdSamples>0)--holdSamples;
            else target=gateCurveAttenuationDb(level,settings);
            gateReduction=smooth(gateReduction,target,target<gateReduction?gateAttack:gateRelease);
            x*=std::pow(10.,-gateReduction/20.);
        }else gateReduction=0;
        if(p[CompOn]>=.5f){
            const auto target=compressorCurveReductionDb(toDb(std::abs(x)),settings);
            compReduction=smooth(compReduction,target,target>compReduction?compAttack:compRelease);
            x*=std::pow(10.,-compReduction/20.)*makeup;
        }else compReduction=0;
        x=(wet==0?dry:wet==1?x:dry*(1-wet)+x*wet)*outputGain;
        if(p[LimiterOn]>=.5f){
            const double required=std::abs(x)>ceiling?ceiling/std::abs(x):1;
            // Instant attack guarantees the CURRENT sample is bounded; release
            // recovers toward unity. This is not a true-peak/lookahead limiter.
            limiterGain=std::min(required,smooth(limiterGain,1.,limiterRelease));
            x=std::clamp(x*limiterGain,-ceiling,ceiling);
        }else limiterGain=1;
        if(!std::isfinite(x)){reset();return 0;}
        constexpr double maximum=std::numeric_limits<float>::max();return static_cast<float>(std::clamp(x,-maximum,maximum));
    }
    float gateAttenuationDb()const noexcept{return static_cast<float>(gateReduction);}
    float compGainReductionDb()const noexcept{return static_cast<float>(compReduction);}
    float limiterGainReductionDb()const noexcept{return static_cast<float>(-20*std::log10(std::max(1e-40,limiterGain)));}
    static constexpr int latencySamples=0;
private:
    static double toDb(double value)noexcept{return 20*std::log10(std::max(1e-12,value));}
    double coefficient(double milliseconds)const noexcept{return std::exp(-1./(milliseconds*.001*sampleRate));}
    static double smooth(double previous,double desired,double amount)noexcept{return desired+(previous-desired)*amount;}
    Snapshot settings;double sampleRate=48000;
    double gateReduction=0,compReduction=0,gateDetector=0,limiterGain=1;
    double gateAttack=0,gateRelease=0,gateDetectorRelease=0,compAttack=0,compRelease=0,limiterRelease=0;
    double wet=1,makeup=1,outputGain=1,ceiling=1;
    int holdSamples=0,holdDuration=0;
};
}
