#pragma once
// Original mono Echo + Reverb insert. No stereo/width or hardware emulation.
// The owner supplies epoch-stamped storage: read(index), write(index, value).
// configure() reports layout/rate changes; the owner MUST invalidate that
// external storage before the next tick when true, and on PANIC/reset.
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>

namespace lah::digitalfx {
constexpr int parameterCount=18;
enum Parameter {
    EchoOn=0,EchoTimeMs,EchoFeedback,EchoMix,EchoLowCut,EchoHighCut,
    ReverbOn,PreDelayMs,DecaySeconds,DampingHz,RoomSize,Diffusion,ReverbMix,
    EchoToReverb,Ducking,EchoMasterDb,ReverbMasterDb,OutputDb
};
struct ParameterSpec {const char* name;const char* unit;float minimum,maximum,step,defaultValue;};
inline const std::array<ParameterSpec,parameterCount>& specs()noexcept{
    static constexpr std::array<ParameterSpec,parameterCount> values{{
        {"Echo on","",0,1,1,1},{"Echo time","ms",5,350,1,180},
        {"Echo feedback","%",0,85,1,25},{"Echo mix","%",0,100,1,18},
        {"Echo low cut","Hz",20,1500,1,80},{"Echo high cut","Hz",500,18000,1,6500},
        {"Reverb on","",0,1,1,1},{"Reverb predelay","ms",0,120,1,15},
        {"Reverb decay RT60","s",.2f,8,.1f,1.2f},{"Reverb damping","Hz",500,18000,1,5500},
        {"Reverb size","",.4f,1,.01f,1},{"Reverb diffusion","%",0,100,1,70},
        {"Reverb mix","%",0,100,1,12},{"Echo to reverb","%",0,100,1,15},
        {"Wet ducking","%",0,100,1,0},{"Echo master","dB",-24,12,.1f,0},
        {"Reverb master","dB",-24,12,.1f,0},{"Output","dB",-24,12,.1f,0}
    }};
    return values;
}
inline std::array<float,parameterCount> defaultParameters()noexcept{
    std::array<float,parameterCount> result{};
    for(std::size_t n=0;n<result.size();++n)result[n]=specs()[n].defaultValue;
    return result;
}
struct Snapshot {
    bool enabled=false; // advanced mode, never the enclosing insert's bypass
    std::array<float,parameterCount> params=defaultParameters();
};
struct Controls {
    std::atomic<bool> enabled{false};
    std::array<std::atomic<float>,parameterCount> params{};
    Controls()noexcept{const auto values=defaultParameters();for(std::size_t n=0;n<values.size();++n)params[n].store(values[n],std::memory_order_relaxed);}
};
inline Snapshot defaults()noexcept{return {};}
inline Snapshot bounded(Snapshot value)noexcept{
    for(std::size_t n=0;n<value.params.size();++n){
        const auto& spec=specs()[n];const auto current=value.params[n];
        value.params[n]=std::isfinite(current)?std::clamp(current,spec.minimum,spec.maximum):spec.defaultValue;
    }
    return value;
}
inline Snapshot snapshot(const Controls& source)noexcept{
    Snapshot result;result.enabled=source.enabled.load(std::memory_order_acquire);
    for(std::size_t n=0;n<result.params.size();++n)result.params[n]=source.params[n].load(std::memory_order_relaxed);
    return bounded(result);
}
inline void recall(Controls& target,Snapshot value)noexcept{
    // A live Pro -> Pro preset must not expose legacy mode for one callback.
    // As with ordinary insert parameters, a callback may observe an intermediate
    // parameter batch, whose bounds and storage layout remain independently safe.
    value=bounded(value);
    for(std::size_t n=0;n<value.params.size();++n)target.params[n].store(value.params[n],std::memory_order_relaxed);
    target.enabled.store(value.enabled,std::memory_order_release);
}
inline void reset(Controls& target)noexcept{recall(target,defaults());}
inline bool equal(const Snapshot& a,const Snapshot& b)noexcept{return a.enabled==b.enabled&&a.params==b.params;}

class Processor {
public:
    // A structural edit resets local histories. The returned true also requires
    // external epoch invalidation; this class never owns or clears delay memory.
    bool configure(Snapshot next,double sampleRate)noexcept{
        next=bounded(next);
        sampleRate=std::isfinite(sampleRate)?std::clamp(sampleRate,8000.0,192000.0):48000.0;
        std::array<std::size_t,8> newLengths{};
        newLengths[0]=samples(sampleRate,next.params[EchoTimeMs]*.001);
        const auto newPredelay=next.params[PreDelayMs]>0?samples(sampleRate,next.params[PreDelayMs]*.001):0;
        newLengths[1]=std::max<std::size_t>(1,newPredelay);
        constexpr std::array<double,6> times{{.0297,.0371,.0411,.0437,.005,.0017}};
        for(std::size_t n=0;n<times.size();++n)newLengths[n+2]=samples(sampleRate,times[n]*(n<4?next.params[RoomSize]:1));
        const bool changed=!configured||sampleRate!=rate||newLengths!=lengths||newPredelay!=predelay||next.enabled!=settings.enabled;
        settings=next;rate=sampleRate;lengths=newLengths;predelay=newPredelay;
        std::size_t offset=0;for(std::size_t n=0;n<lengths.size();++n){offsets[n]=offset;offset+=lengths[n];}needed=offset;
        echoHp=lowpassCoefficient(next.params[EchoLowCut],rate);
        echoLp=lowpassCoefficient(next.params[EchoHighCut],rate);
        damping=lowpassCoefficient(next.params[DampingHz],rate);
        envelopeAttack=std::exp(-1.0/(rate*.005));envelopeRelease=std::exp(-1.0/(rate*.180));
        for(std::size_t n=0;n<combFeedback.size();++n)
            combFeedback[n]=std::pow(10.0,-3.0*lengths[n+2]/(rate*next.params[DecaySeconds]));
        echoGain=db(next.params[EchoMasterDb]);reverbGain=db(next.params[ReverbMasterDb]);outputGain=db(next.params[OutputDb]);
        configured=true;if(changed)reset();return changed;
    }
    void reset()noexcept{
        positions.fill(0);combDamping.fill(0);echoLowState=echoHighState=envelope=0;
    }
    std::size_t requiredSamples()const noexcept{return needed;}
    std::size_t echoSamples()const noexcept{return lengths[0];}
    std::size_t preDelaySamples()const noexcept{return predelay;}
    double sampleRate()const noexcept{return rate;}

    template<class Storage>float tick(float input,Storage& storage)noexcept{
        const float dry=clean(input);
        if(!configured||!settings.enabled)return dry;
        const double x=std::clamp(static_cast<double>(dry),-16.0,16.0);
        const auto& p=settings.params;
        const bool echoOn=p[EchoOn]>=.5f,reverbOn=p[ReverbOn]>=.5f;
        const double echoRead=clean(storage.read(offsets[0]+positions[0]));
        echoLowState=flush(echoHp*echoLowState+(1-echoHp)*echoRead);
        const double highPassed=echoRead-echoLowState;
        echoHighState=flush(echoLp*echoHighState+(1-echoLp)*highPassed);
        const double echo=echoHighState;
        write(storage,offsets[0]+positions[0],(echoOn?x:0)+echo*p[EchoFeedback]*.01);
        advance(0);

        double reverbInput=reverbOn?x+(echoOn?echo*echoGain*p[EchoToReverb]*.01:0):0;
        if(predelay>0){
            const auto index=offsets[1]+positions[1];const double delayed=clean(storage.read(index));
            write(storage,index,reverbInput);reverbInput=delayed;advance(1);
        }
        double reverb=0;
        for(std::size_t comb=0;comb<combFeedback.size();++comb){
            const auto part=comb+2,index=offsets[part]+positions[part];
            const double delayed=clean(storage.read(index));
            combDamping[comb]=flush(damping*combDamping[comb]+(1-damping)*delayed);
            write(storage,index,reverbInput+combDamping[comb]*combFeedback[comb]);
            reverb+=delayed*.25;advance(part);
        }
        // Two allpasses continuously run; diffusion blends each with its input.
        // Zero diffusion is the un-diffused comb bank, without a frozen history.
        for(std::size_t part=6;part<8;++part){
            const auto index=offsets[part]+positions[part];const double delayed=clean(storage.read(index));
            const double diffused=delayed-.7*reverb;write(storage,index,reverb+.7*diffused);
            reverb+=(diffused-reverb)*p[Diffusion]*.01;advance(part);
        }
        const double level=std::abs(x),coefficient=level>envelope?envelopeAttack:envelopeRelease;
        envelope=flush(coefficient*envelope+(1-coefficient)*level);
        const double wetGain=1.0/(1+8*p[Ducking]*.01*envelope);
        const double echoMix=echoOn?p[EchoMix]*.01:0,reverbMix=reverbOn?p[ReverbMix]*.01:0;
        // Keep the actual dry bits, including negative zero, while delay histories
        // still run so increasing a mix does not reveal frozen stale memory.
        if(echoMix==0&&reverbMix==0&&outputGain==1)return dry;
        const double mixed=dry*(1-std::max(echoMix,reverbMix))
            +wetGain*(echo*echoGain*echoMix+reverb*reverbGain*reverbMix);
        return clean(static_cast<float>(mixed*outputGain));
    }
private:
    static std::size_t samples(double fs,double seconds)noexcept{return std::max<std::size_t>(1,static_cast<std::size_t>(std::llround(fs*seconds)));}
    static double lowpassCoefficient(double hz,double fs)noexcept{return std::exp(-6.2831853071795864769*std::min(hz,fs*.45)/fs);}
    static double db(double value)noexcept{return std::pow(10.0,value*.05);}
    static float clean(float value)noexcept{return std::isfinite(value)?value:0;}
    static double flush(double value)noexcept{return !std::isfinite(value)||std::abs(value)<1e-25?0:value;}
    template<class Storage>static void write(Storage& storage,std::size_t index,double value)noexcept{
        storage.write(index,static_cast<float>(std::clamp(std::isfinite(value)?value:0.0,-16.0,16.0)));
    }
    void advance(std::size_t part)noexcept{if(++positions[part]==lengths[part])positions[part]=0;}
    Snapshot settings{};
    bool configured=false;
    double rate=48000,echoHp=0,echoLp=0,damping=0,envelopeAttack=0,envelopeRelease=0;
    double echoGain=1,reverbGain=1,outputGain=1,echoLowState=0,echoHighState=0,envelope=0;
    std::array<double,4> combFeedback{},combDamping{};
    std::array<std::size_t,8> lengths{},offsets{},positions{};
    std::size_t needed=0,predelay=0;
};
} // namespace lah::digitalfx
