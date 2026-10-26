#pragma once
// Native mono channel inserts. No JUCE, allocation, locks, or buffer clearing in the audio path.
// These are explicitly described analytic processors, not third-party product emulations or ML.
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <memory>
#include "FeedbackSuppressionCore.h"
#include "ExtendedEq.h"
#include "NativeSpectrum.h"
#include "DigitalFxPro.h"
#include "DynamicsPro.h"

namespace lah::builtin {
constexpr int channelCount=32,slotCount=4,parameterCount=6,moduleCount=17;
constexpr double pi=3.14159265358979323846;
enum ModuleId {None=0,VocalClean=1,VocalColor=2,FeedbackGuard=3,AutoFeedback=4,Equalizer=5,
    SmartEq=6,Dynamics=7,SmartComp=8,OpticalComp=9,Fx=10,DigitalFx=11,Reverb96=12,
    SubPro=13,DrumLive=14,PcAudio=15,Analog=16,FourKLine=17};
struct ParameterInfo {const char* name;const char* unit;float minimum,maximum,step,defaultValue;};
struct ModuleInfo {const char* name;const char* description;int parameterCount;std::array<ParameterInfo,6> params;};
inline const ModuleInfo& metadata(int id)noexcept {
    static const std::array<ModuleInfo,moduleCount+1> catalog {{
        {"None","Direct bypass; no processor.",0,{}},
        {"Vocal Clean","High-pass, 250 Hz bell cut, dynamic high-band de-esser, peak compressor.",6,{{
            {"High-pass","Hz",40,300,1,90},{"Low-mid cut","dB",-12,0,.1f,-3},{"De-ess strength","",0,12,.1f,3},
            {"Threshold","dB",-40,0,.1f,-18},{"Ratio",":1",1,8,.1f,3},{"Makeup","dB",-12,12,.1f,0}}}},
        {"Vocal Color","High-pass, warmth/presence bell EQ and soft saturation; no hardware emulation.",6,{{
            {"High-pass","Hz",30,250,1,80},{"Warmth","dB",-6,9,.1f,2},{"Presence","dB",-6,9,.1f,1},
            {"Drive","dB",0,18,.1f,3},{"Wet mix","%",0,100,1,100},{"Output","dB",-12,12,.1f,0}}}},
        {"AI-FB LAB","Synthetic-trained neural model, spectral/growth detection and armed automatic notch bank; manual EQ remains available.",6,{{
            {"Notch frequency","Hz",80,12000,1,1000},{"Notch depth","dB",-24,0,.1f,0},{"Notch Q","",2,40,.1f,12},
            {"High-pass","Hz",20,300,1,40},{"Clip ceiling","dBFS",-18,0,.1f,-3},{"Wet mix","%",0,100,1,100}}}},
        {"AUTO FB PRO","Spectral/growth detector and armed automatic notch bank. Music can trigger detection. Manual EQ remains available.",6,{{
            {"Manual notch","Hz",80,12000,1,1000},{"Manual depth","dB",-24,0,.1f,0},{"Notch Q","",2,40,.1f,20},
            {"Persistence","ms",300,3000,10,800},{"Tone advisory tolerance","%",1,15,.1f,5},{"Output","dB",-12,6,.1f,0}}}},
        {"Equalizer","Legacy HPF/three bell bands, or optional eight independent bell/shelf/HPF/LPF bands.",6,{{
            {"High-pass","Hz",20,500,1,20},{"Low 100 Hz","dB",-12,12,.1f,0},{"Mid frequency","Hz",100,12000,1,1000},
            {"Mid gain","dB",-18,18,.1f,0},{"Mid Q","",.2f,10,.1f,1},{"High 8 kHz","dB",-12,12,.1f,0}}}},
        {"Smart EQ","Heuristic three-band energy-dependent cuts; operator presence EQ. No ML or target matching.",6,{{
            {"High-pass","Hz",20,300,1,80},{"Low-mid control","dB",0,12,.1f,3},{"Presence","dB",-6,6,.1f,1},
            {"Harshness control","dB",0,12,.1f,2},{"Adaptation","ms",50,1000,1,300},{"Output","dB",-12,6,.1f,0}}}},
        {"Dynamics","Peak feed-forward compressor and downward expander with attack/release envelope.",6,{{
            {"Threshold","dB",-60,0,.1f,-18},{"Ratio",":1",1,20,.1f,3},{"Attack","ms",.2f,100,.1f,10},
            {"Release","ms",10,1500,1,120},{"Gate threshold","dB",-100,-10,1,-70},{"Makeup","dB",-12,18,.1f,0}}}},
        {"Smart Compressor","RMS compressor with level-dependent release and parallel wet mix. No ML.",6,{{
            {"Threshold","dB",-60,0,.1f,-18},{"Ratio",":1",1,12,.1f,3},{"Attack","ms",1,100,1,10},
            {"Release","ms",30,1500,1,200},{"Wet mix","%",0,100,1,100},{"Makeup","dB",-12,18,.1f,0}}}},
        {"Optical Compressor","Dual-time-constant peak envelope and optional soft saturation; analytic character model.",6,{{
            {"Threshold","dB",-50,0,.1f,-20},{"Ratio",":1",1,10,.1f,4},{"Attack","ms",2,80,1,12},
            {"Release","ms",100,2500,1,500},{"Drive","dB",0,12,.1f,0},{"Makeup","dB",-12,18,.1f,0}}}},
        {"FX Delay","Mono feedback delay with damping and optional soft saturation; causal dry/wet mix.",6,{{
            {"Wet mix","%",0,100,1,25},{"Delay","ms",10,600,1,180},{"Feedback","",0,.8f,.01f,.25f},
            {"Damping","Hz",500,16000,1,6500},{"Drive","dB",0,12,.1f,0},{"Output","dB",-12,6,.1f,0}}}},
        {"Digital FX","Sample hold and bit-depth quantizer, mixed with a short feedback echo.",6,{{
            {"Bit depth","bits",4,24,1,12},{"Sample hold","samples",1,32,1,1},{"Wet mix","%",0,100,1,20},
            {"Echo delay","ms",10,400,1,80},{"Feedback","",0,.7f,.01f,.15f},{"Output","dB",-12,6,.1f,0}}}},
        {"Reverb 96","Four damped Schroeder combs, two allpasses and predelay; native mono algorithmic reverb.",6,{{
            {"Wet mix","%",0,100,1,20},{"Decay RT60","s",.2f,5,.1f,1.2f},{"Damping","Hz",500,16000,1,5500},
            {"Predelay","ms",0,100,1,15},{"Room scale","",.3f,1.5f,.01f,1},{"Output","dB",-12,6,.1f,0}}}},
        {"Sub Pro","High-pass, fourth-order low-pass, low bell, polarity and optional soft saturation.",6,{{
            {"High-pass","Hz",15,80,1,30},{"Low-pass","Hz",40,250,1,110},{"55 Hz weight","dB",-9,9,.1f,3},
            {"Invert polarity","",0,1,1,0},{"Drive","dB",0,12,.1f,0},{"Output","dB",-12,6,.1f,-3}}}},
        {"Drum Live","High-pass, body/attack bell EQ, envelope gate and transient emphasis.",6,{{
            {"High-pass","Hz",20,250,1,35},{"Body 90 Hz","dB",-9,9,.1f,3},{"Attack 4.5 kHz","dB",-9,9,.1f,2},
            {"Gate threshold","dB",-90,-10,1,-50},{"Gate release","ms",20,1000,1,150},{"Transient boost","dB",0,9,.1f,3}}}},
        {"PC Audio","High-pass, bass/treble bell EQ, peak compressor and sample clip guard.",6,{{
            {"High-pass","Hz",20,150,1,35},{"Bass 100 Hz","dB",-9,9,.1f,1},{"Treble 7 kHz","dB",-9,9,.1f,1},
            {"Threshold","dB",-36,0,.1f,-12},{"Ratio",":1",1,8,.1f,2},{"Clip ceiling","dBFS",-12,0,.1f,-.5f}}}},
        {"Analog","Native high-pass, broad warmth/air EQ and soft saturation. Analytic design; no hardware emulation.",6,{{
            {"High-pass","Hz",20,250,1,35},{"Warmth 150 Hz","dB",-9,9,.1f,1},{"Air 10 kHz","dB",-9,9,.1f,.5f},
            {"Drive","dB",0,18,.1f,3},{"Wet mix","%",0,100,1,100},{"Output","dB",-12,12,.1f,0}}}},
        {"4K Line","Native high-pass, four fixed-frequency bell bands and line saturation. Analytic channel strip; no console emulation.",6,{{
            {"High-pass","Hz",20,350,1,30},{"Low 100 Hz","dB",-12,12,.1f,0},{"Low-mid 400 Hz","dB",-12,12,.1f,0},
            {"Presence 3 kHz","dB",-12,12,.1f,0},{"Air 10 kHz","dB",-12,12,.1f,0},{"Drive","dB",0,18,.1f,0}}}}
    }};
    return catalog[static_cast<std::size_t>(std::clamp(id,0,moduleCount))];
}
struct SlotControl {
    std::atomic<int> type {None};
    std::atomic<bool> enabled {false};
    std::array<std::atomic<float>,6> params {};
    digitalfx::Controls digitalFxPro;
    dynamicspro::Controls dynamicsPro;
    std::atomic<bool> extendedEq{false},eqInitialized{false};
    std::array<EqBandControl,eqBandCount> eqBands{};
    std::atomic<bool> feedbackArmed {false};
    std::atomic<std::uint32_t> feedbackResetGeneration {0};
};
struct Controls {
    std::array<std::array<SlotControl,slotCount>,channelCount> slots {};
    auto& operator[](std::size_t channel)noexcept{return slots[channel];}
    const auto& operator[](std::size_t channel)const noexcept{return slots[channel];}
};
inline void resetExtendedEq(SlotControl& slot)noexcept {
    slot.extendedEq.store(false,std::memory_order_release);slot.eqInitialized.store(false,std::memory_order_relaxed);
    const auto defaults=defaultEqBands();for(std::size_t band=0;band<defaults.size();++band)recallEqBand(slot.eqBands[band],defaults[band]);
}
// First conversion reproduces the legacy HPF + three bell sections. Returning
// to legacy mode never erases edits made to the separate eight-band state.
inline bool setExtendedEqEnabled(SlotControl& slot,bool enabled)noexcept {
    if(slot.type.load(std::memory_order_acquire)!=Equalizer)return false;
    if(enabled && !slot.eqInitialized.load(std::memory_order_acquire)){
        const auto& info=metadata(Equalizer);std::array<float,6> p{};
        for(std::size_t n=0;n<p.size();++n){const auto value=slot.params[n].load(std::memory_order_acquire);p[n]=std::isfinite(value)?std::clamp(value,info.params[n].minimum,info.params[n].maximum):info.params[n].defaultValue;}
        auto bands=defaultEqBands();bands[0]={true,EqHighPass,p[0],0,.70710678f};
        bands[1]={true,EqBell,100,p[1],.7f};bands[3]={true,EqBell,p[2],p[3],p[4]};bands[5]={true,EqBell,8000,p[5],.7f};
        for(std::size_t band=0;band<bands.size();++band)recallEqBand(slot.eqBands[band],bands[band]);
        slot.eqInitialized.store(true,std::memory_order_release);
    }
    slot.extendedEq.store(enabled,std::memory_order_release);return true;
}
inline void configureSlot(SlotControl& slot,int type,bool enabled=false)noexcept {
    slot.feedbackArmed.store(false,std::memory_order_release);
    slot.feedbackResetGeneration.fetch_add(1,std::memory_order_acq_rel);
    slot.enabled.store(false,std::memory_order_relaxed);
    digitalfx::reset(slot.digitalFxPro);dynamicspro::reset(slot.dynamicsPro);
    resetExtendedEq(slot);
    type=std::clamp(type,0,moduleCount);
    const auto& info=metadata(type);
    for(int n=0;n<parameterCount;++n)slot.params[static_cast<std::size_t>(n)].store(info.params[static_cast<std::size_t>(n)].defaultValue,std::memory_order_relaxed);
    slot.type.store(type,std::memory_order_relaxed);
    slot.enabled.store(enabled,std::memory_order_release);
}
inline float finite(float x)noexcept{return std::isfinite(x)?x:0.0f;}
inline float gain(float db)noexcept{return std::pow(10.0f,db*.05f);}
namespace detail {
struct Biquad {
    double b0=1,b1=0,b2=0,a1=0,a2=0,z1=0,z2=0;
    void reset()noexcept{z1=z2=0;}
    void configure(double B0,double B1,double B2,double A0,double A1,double A2)noexcept {
        b0=B0/A0;b1=B1/A0;b2=B2/A0;a1=A1/A0;a2=A2/A0;
    }
    void filter(double hz,double q,double db,double fs,int kind)noexcept {
        const double w=2*pi*std::clamp(hz,10.0,fs*.45)/fs,c=std::cos(w),a=std::sin(w)/(2*q);
        if(kind==0)configure((1+c)*.5,-(1+c),(1+c)*.5,1+a,-2*c,1-a);
        else if(kind==1)configure((1-c)*.5,1-c,(1-c)*.5,1+a,-2*c,1-a);
        else {const double A=std::pow(10.0,db/40.0);configure(1+a*A,-2*c,1-a*A,1+a/A,-2*c,1-a/A);}
    }
    float tick(float x)noexcept {
        const double y=b0*x+z1;z1=b1*x-a1*y+z2;z2=b2*x-a2*y;
        if(!std::isfinite(y)||!std::isfinite(z1)||!std::isfinite(z2)){reset();return 0;}
        if(std::abs(z1)<1e-25)z1=0;if(std::abs(z2)<1e-25)z2=0;
        return finite(static_cast<float>(y));
    }
};
struct State {
    int type=None;bool enabled=false;
    std::array<float,6> p {};
    digitalfx::Snapshot digitalFxSettings;
    dynamicspro::Snapshot dynamicsSettings;
    digitalfx::Processor digitalFxProcessor;
    dynamicspro::Processor dynamicsProcessor;
    float dynamicsFinalCeiling=1;
    bool dynamicsFinalLimited=false;
    std::array<float,6> gains {};
    std::array<Biquad,5> filters {};
    bool extendedEq=false;
    std::array<EqBandSnapshot,eqBandCount> eqSettings{};
    std::array<EqFilter,eqBandCount> eqFilters{};
    std::array<float,8> env {};
    std::vector<float> delay;
    std::vector<std::uint64_t> stamps;
    std::uint64_t delayEpoch=1,globalEpoch=0,channelEpoch=0;
    std::size_t position=0;
    std::array<std::size_t,7> lengths {},offsets {},positions {};
    std::array<float,4> combFeedback {};
    float attack=.99f,release=.999f,slowRelease=.9999f,damping=.5f,extraAttack=.99f,extraRelease=.999f;
    float previousInput=0,previousOutput=0,transitionDelta=0,held=0,period=0,stableSamples=0,lastCandidate=0;
    unsigned ramp=64,holdCounter=0,crossSamples=0;
    void clear()noexcept {
        digitalFxProcessor.reset();dynamicsProcessor.reset();
        for(auto& filter:filters)filter.reset();env.fill(0);
        for(auto& filter:eqFilters)filter.reset();
        ++delayEpoch;if(delayEpoch==0)++delayEpoch;
        position=0;positions.fill(0);previousInput=previousOutput=transitionDelta=held=period=stableSamples=lastCandidate=0;
        holdCounter=crossSamples=0;ramp=64;
    }
    float read(std::size_t n)const noexcept{return stamps[n]==delayEpoch?delay[n]:0.0f;}
    void write(std::size_t n,float value)noexcept{delay[n]=std::clamp(finite(value),-16.0f,16.0f);stamps[n]=delayEpoch;}
};
}

using FeedbackStatus=feedback::Status;
struct MeterLevels {
    float inputPeak=0,outputPeak=0;
    std::uint64_t generation=0; // zero until this slot has completed a measured block
};
class Rack {
public:
    // Attach only before callback startup; service lifetime must cover the rack.
    void setSpectrumTap(spectrum::Capture* tap,bool outputDomain=false)noexcept{spectrumTap=tap;spectrumOutput=outputDomain;}
    void prepare(double sampleRate,int maxBlock=512) {
        if(!std::isfinite(sampleRate)||sampleRate<8000||sampleRate>192000||maxBlock<1)
            throw std::invalid_argument("Builtin rack requires 8-192 kHz and a positive maximum block size");
        fs=sampleRate;
        feedbackStates=std::make_unique<feedback::Processor[]>(channelCount*slotCount);
        for(int i=0;i<channelCount*slotCount;++i)feedbackStates[static_cast<std::size_t>(i)].prepare(sampleRate,(i*37)%256);
        const auto size=static_cast<std::size_t>(std::ceil(fs*.65))+16;
        for(auto& channel:states)for(auto& state:channel) {
            state.delay.assign(size,0.0f);state.stamps.assign(size,0);state.clear();
            state.type=None;state.enabled=false;state.globalEpoch=0;
        }
        prepared=true;reset();
    }
    // The entire rack is invalidated by one scalar increment. States clear lazily when touched.
    void reset()noexcept{++globalEpoch;if(globalEpoch==0)++globalEpoch;publishedEpoch.store(globalEpoch,std::memory_order_release);if(spectrumTap)spectrumTap->invalidateDomain(spectrumOutput);}
    // May be called by the control thread during PANIC. Sample accumulators
    // remain callback-owned; their next publication rejects this old epoch.
    void invalidateMeters()noexcept{meterVisibilityEpoch.fetch_add(1,std::memory_order_release);if(spectrumTap)spectrumTap->invalidateDomain(spectrumOutput);}
    void resetChannel(int channel)noexcept {
        if(channel>=0&&channel<channelCount){
            if(spectrumTap)spectrumTap->invalidateTarget(spectrumOutput,channel);
            auto& epoch=channelEpochs[static_cast<std::size_t>(channel)];++epoch;if(epoch==0)++epoch;
            publishedChannelEpochs[static_cast<std::size_t>(channel)].store(epoch,std::memory_order_release);
        }
    }
    void beginBlock(Controls& controls)noexcept {
        currentControls.store(&controls,std::memory_order_release);
        const auto visibility=meterVisibilityEpoch.load(std::memory_order_acquire);
        callbackVisibilityEpoch=visibility;
        if(++meterBlockNumber==0)++meterBlockNumber;
        for(int ch=0;ch<channelCount;++ch)for(int slot=0;slot<slotCount;++slot) {
            auto& state=states[static_cast<std::size_t>(ch)][static_cast<std::size_t>(slot)];
            auto& control=controls.slots[static_cast<std::size_t>(ch)][static_cast<std::size_t>(slot)];
            const auto feedbackIndex=static_cast<std::size_t>(ch*slotCount+slot);
            publishMeter(feedbackIndex,ch,visibility);
            meterAccumulators[feedbackIndex]={0,0,globalEpoch,channelEpochs[static_cast<std::size_t>(ch)],visibility,meterBlockNumber,false};
            int type=control.type.load(std::memory_order_relaxed);if(type<None||type>moduleCount)type=None;
            const bool enabled=control.enabled.load(std::memory_order_acquire)&&type!=None;
            const bool invalid=state.globalEpoch!=globalEpoch||state.channelEpoch!=channelEpochs[static_cast<std::size_t>(ch)];
            const bool moduleChanged=type!=state.type||enabled!=state.enabled;
            if((invalid||moduleChanged)&&spectrumTap)spectrumTap->invalidateTarget(spectrumOutput,ch,slot);
            if(invalid){state.clear();state.globalEpoch=globalEpoch;state.channelEpoch=channelEpochs[static_cast<std::size_t>(ch)];feedbackStates[feedbackIndex].reset();control.feedbackArmed.store(false,std::memory_order_release);}
            if(moduleChanged) {
                const float delta=invalid?0.0f:state.previousOutput-state.previousInput;
                state.clear();state.transitionDelta=delta;state.ramp=invalid?64u:0u;
                state.type=type;state.enabled=enabled;
                feedbackStates[feedbackIndex].reset();control.feedbackArmed.store(false,std::memory_order_release);
            }
            if(invalid||!enabled||type!=AutoFeedback) {
                const auto index=static_cast<std::size_t>(ch*slotCount+slot);
                candidates[index].store(0,std::memory_order_relaxed);
                candidateEpochs[index].store(globalEpoch,std::memory_order_release);
                candidateChannelEpochs[index].store(channelEpochs[static_cast<std::size_t>(ch)],std::memory_order_release);
                candidateVisibilityEpochs[index].store(callbackVisibilityEpoch,std::memory_order_release);
            }
            // A disabled slot only needs its dry/delta transition; parameters are snapped on re-enable.
            if(!enabled)continue;
            const auto& info=metadata(type);
            bool parametersChanged=false,reverbStructureChanged=false;
            const auto nextDigital=type==DigitalFx?digitalfx::snapshot(control.digitalFxPro):digitalfx::Snapshot{};
            const auto nextDynamics=type==Dynamics?dynamicspro::snapshot(control.dynamicsPro):dynamicspro::Snapshot{};
            const bool digitalChanged=!digitalfx::equal(state.digitalFxSettings,nextDigital);
            const bool dynamicsChanged=state.dynamicsSettings!=nextDynamics;
            if(invalid||moduleChanged||digitalChanged){
                const bool layoutChanged=state.digitalFxProcessor.configure(nextDigital,fs);
                reverbStructureChanged=reverbStructureChanged||((state.digitalFxSettings.enabled||nextDigital.enabled)&&(layoutChanged||state.digitalFxSettings.enabled!=nextDigital.enabled));
            }
            if(invalid||moduleChanged||dynamicsChanged){
                state.dynamicsProcessor.configure(nextDynamics,fs);
                state.dynamicsFinalLimited=nextDynamics.enabled&&nextDynamics.params[dynamicspro::LimiterOn]>=.5f;
                state.dynamicsFinalCeiling=state.dynamicsFinalLimited?gain(nextDynamics.params[dynamicspro::LimiterCeiling]):1.0f;
            }
            parametersChanged=parametersChanged||(digitalChanged&&(state.digitalFxSettings.enabled||nextDigital.enabled))||
                (dynamicsChanged&&(state.dynamicsSettings.enabled||nextDynamics.enabled));
            state.digitalFxSettings=nextDigital;state.dynamicsSettings=nextDynamics;
            for(int p=0;p<parameterCount;++p) {
                const auto& spec=info.params[static_cast<std::size_t>(p)];
                const float value=control.params[static_cast<std::size_t>(p)].load(std::memory_order_relaxed);
                const float snapped=std::isfinite(value)?std::clamp(value,spec.minimum,spec.maximum):spec.defaultValue;
                if(snapped!=state.p[static_cast<std::size_t>(p)]){
                    parametersChanged=true;
                    reverbStructureChanged=reverbStructureChanged||(type==Reverb96&&(p==3||p==4));
                }
                state.p[static_cast<std::size_t>(p)]=snapped;
                state.gains[static_cast<std::size_t>(p)]=gain(std::clamp(state.p[static_cast<std::size_t>(p)],-100.0f,36.0f));
            }
            const bool useExtended=type==Equalizer && control.extendedEq.load(std::memory_order_acquire);
            if(state.extendedEq!=useExtended){state.extendedEq=useExtended;parametersChanged=true;for(auto& filter:state.eqFilters)filter.reset();}
            if(useExtended)for(std::size_t band=0;band<state.eqSettings.size();++band){
                const auto next=snapshotEqBand(control.eqBands[band]);const auto previous=state.eqSettings[band];
                const bool changed=next!=previous;
                if(changed)parametersChanged=true;
                if(next.enabled!=previous.enabled || next.type!=previous.type)state.eqFilters[band].reset();
                state.eqSettings[band]=next;
                if(changed || invalid || moduleChanged)state.eqFilters[band].configure(next,fs);
            }
            if(parametersChanged && !invalid && !moduleChanged){
                const float previousInput=state.previousInput,previousOutput=state.previousOutput;
                if(reverbStructureChanged){
                    // New partition lengths must not reinterpret an old tail.
                    // Clearing also discards the comb damping envelopes; ordinary
                    // EQ/dynamics parameter edits retain their filter histories.
                    state.clear();state.previousInput=previousInput;state.previousOutput=previousOutput;
                }
                state.transitionDelta=previousOutput-previousInput;state.ramp=0;
            }
            configure(state);
            if(type==FeedbackGuard||type==AutoFeedback){
                feedback::Settings settings;settings.armed=control.feedbackArmed.load(std::memory_order_acquire);
                settings.useModel=type==FeedbackGuard;settings.persistenceMs=type==AutoFeedback?state.p[3]:350.f;
                settings.resetGeneration=control.feedbackResetGeneration.load(std::memory_order_acquire);
                feedbackStates[feedbackIndex].configure(settings);
            }
        }
        if(spectrumTap)spectrumTap->beginBlock(spectrumOutput);
    }
    float processSample(int channel,float input)noexcept {
        if(channel<0||channel>=channelCount)return 0.0f;
        float x=finite(input);
        if(!prepared)return x;
        for(int slot=0;slot<slotCount;++slot) {
            auto& state=states[static_cast<std::size_t>(channel)][static_cast<std::size_t>(slot)];
            if(state.globalEpoch!=globalEpoch||state.channelEpoch!=channelEpochs[static_cast<std::size_t>(channel)]) {
                state.clear();state.globalEpoch=globalEpoch;state.channelEpoch=channelEpochs[static_cast<std::size_t>(channel)];
                feedbackStates[static_cast<std::size_t>(channel*slotCount+slot)].reset();
                if(auto* controls=currentControls.load(std::memory_order_relaxed))controls->slots[static_cast<std::size_t>(channel)][static_cast<std::size_t>(slot)].feedbackArmed.store(false,std::memory_order_release);
            }
            const float dry=x;
            float wet=state.enabled?process(state,std::clamp(x,-16.0f,16.0f),channel,slot):x;
            if(state.enabled&&(state.type==FeedbackGuard||state.type==AutoFeedback)){
                const auto index=static_cast<std::size_t>(channel*slotCount+slot);
                auto& feedback=feedbackStates[index];wet=feedback.processSample(wet);
                if(feedbackRevision[index]!=feedback.revision()){
                    feedbackRevision[index]=feedback.revision();publishFeedback(index,feedback.status(),channel);
                }
            }
            const bool transitioning=state.ramp<64;
            if(state.ramp<64) {
                const float t=static_cast<float>(++state.ramp)/64.0f;
                x=dry+state.transitionDelta*(1-t)+(wet-dry)*t;
            }else x=wet;
            x=state.enabled||transitioning?std::clamp(finite(x),-16.0f,16.0f):finite(x);
            float limiterTransitionReduction=0;
            if(state.enabled&&state.type==Dynamics&&state.dynamicsFinalLimited){
                const float before=std::abs(x);x=std::clamp(x,-state.dynamicsFinalCeiling,state.dynamicsFinalCeiling);
                if(before>state.dynamicsFinalCeiling)limiterTransitionReduction=20*std::log10(before/state.dynamicsFinalCeiling);
            }
            auto& meter=meterAccumulators[static_cast<std::size_t>(channel*slotCount+slot)];
            if(meter.epoch!=globalEpoch||meter.channelEpoch!=channelEpochs[static_cast<std::size_t>(channel)]){
                meter.inputPeak=meter.outputPeak=0;meter.epoch=globalEpoch;
                meter.reductionDb.fill(0);meter.proDynamics=false;
                meter.channelEpoch=channelEpochs[static_cast<std::size_t>(channel)];
            }
            meter.inputPeak=std::max(meter.inputPeak,std::abs(dry));
            meter.outputPeak=std::max(meter.outputPeak,std::abs(x));meter.touched=true;
            if(state.enabled&&state.type==Dynamics&&state.dynamicsSettings.enabled){
                meter.proDynamics=true;
                meter.reductionDb[0]=std::max(meter.reductionDb[0],state.dynamicsProcessor.gateAttenuationDb());
                meter.reductionDb[1]=std::max(meter.reductionDb[1],state.dynamicsProcessor.compGainReductionDb());
                meter.reductionDb[2]=std::max(meter.reductionDb[2],std::max(state.dynamicsProcessor.limiterGainReductionDb(),limiterTransitionReduction));
            }
            if(spectrumTap)spectrumTap->push(spectrumOutput,channel,slot,dry,x);
            state.previousInput=dry;state.previousOutput=x;
        }
        return x;
    }
    // Read-only UI telemetry, independent of the main console's consuming peak
    // meters. Packed publication keeps the pre/post pair from one measured block.
    // beginBlock publishes the completed previous block; no atomics per sample.
    MeterLevels meterLevels(int channel,int slot)const noexcept {
        if(channel<0||channel>=channelCount||slot<0||slot>=slotCount)return {};
        const auto& published=metersPublished[static_cast<std::size_t>(channel*slotCount+slot)];
        const auto epoch=publishedEpoch.load(std::memory_order_acquire);
        const auto channelEpoch=publishedChannelEpochs[static_cast<std::size_t>(channel)].load(std::memory_order_acquire);
        const auto visibility=meterVisibilityEpoch.load(std::memory_order_acquire);
        if(published.epoch.load(std::memory_order_acquire)!=epoch
            ||published.channelEpoch.load(std::memory_order_acquire)!=channelEpoch
            ||published.visibility.load(std::memory_order_acquire)!=visibility)return {};
        const auto packed=published.peaks.load(std::memory_order_acquire);
        MeterLevels result;
        const auto inputBits=static_cast<std::uint32_t>(packed),outputBits=static_cast<std::uint32_t>(packed>>32);
        std::memcpy(&result.inputPeak,&inputBits,sizeof(float));
        std::memcpy(&result.outputPeak,&outputBits,sizeof(float));
        result.generation=published.generation.load(std::memory_order_relaxed);
        if(epoch!=publishedEpoch.load(std::memory_order_acquire)
            ||channelEpoch!=publishedChannelEpochs[static_cast<std::size_t>(channel)].load(std::memory_order_acquire)
            ||visibility!=meterVisibilityEpoch.load(std::memory_order_acquire))return {};
        return result;
    }
    dynamicspro::GainReduction dynamicsProReduction(int channel,int slot)const noexcept {
        if(channel<0||channel>=channelCount||slot<0||slot>=slotCount)return {};
        const auto index=static_cast<std::size_t>(channel*slotCount+slot);const auto& p=metersPublished[index];
        const auto sequence=p.sequence.load(std::memory_order_acquire);if(sequence&1)return {};
        const auto epoch=publishedEpoch.load(std::memory_order_acquire),visibility=meterVisibilityEpoch.load(std::memory_order_acquire);
        const auto channelEpoch=publishedChannelEpochs[static_cast<std::size_t>(channel)].load(std::memory_order_acquire);
        if(!p.proDynamics.load(std::memory_order_acquire)||p.epoch.load()!=epoch||p.visibility.load()!=visibility||p.channelEpoch.load()!=channelEpoch)return {};
        const auto* c=currentControls.load(std::memory_order_acquire);
        if(!c)return {};const auto& slotControl=c->slots[static_cast<std::size_t>(channel)][static_cast<std::size_t>(slot)];
        if(slotControl.type.load()!=Dynamics||!slotControl.enabled.load()||!slotControl.dynamicsPro.enabled.load())return {};
        dynamicspro::GainReduction result;result.gateDb=p.reductionDb[0].load();result.compressorDb=p.reductionDb[1].load();result.limiterDb=p.reductionDb[2].load();result.generation=p.generation.load();
        if(sequence!=p.sequence.load(std::memory_order_acquire)||epoch!=publishedEpoch.load(std::memory_order_acquire)||visibility!=meterVisibilityEpoch.load(std::memory_order_acquire)||channelEpoch!=publishedChannelEpochs[static_cast<std::size_t>(channel)].load(std::memory_order_acquire))return {};
        return result;
    }
    // Advisory only: a sustained musical tone can also satisfy the detector.
    float feedbackCandidateHz(int channel,int slot)const noexcept {
        if(channel<0||channel>=channelCount||slot<0||slot>=slotCount)return 0;
        const auto index=static_cast<std::size_t>(channel*slotCount+slot);
        const auto visibility=meterVisibilityEpoch.load(std::memory_order_acquire);
        if(candidateEpochs[index].load(std::memory_order_acquire)!=publishedEpoch.load(std::memory_order_acquire)
            ||candidateChannelEpochs[index].load(std::memory_order_acquire)!=publishedChannelEpochs[static_cast<std::size_t>(channel)].load(std::memory_order_acquire)
            ||candidateVisibilityEpochs[index].load(std::memory_order_acquire)!=visibility)return 0;
        const auto result=candidates[index].load(std::memory_order_relaxed);
        return visibility==meterVisibilityEpoch.load(std::memory_order_acquire)?result:0;
    }
    FeedbackStatus feedbackStatus(int channel,int slot)const noexcept {
        FeedbackStatus result;
        if(channel<0||channel>=channelCount||slot<0||slot>=slotCount)return result;
        const auto index=static_cast<std::size_t>(channel*slotCount+slot);const auto& p=feedbackPublished[index];
        const auto visibility=meterVisibilityEpoch.load(std::memory_order_acquire);
        if(p.epoch.load(std::memory_order_acquire)!=publishedEpoch.load(std::memory_order_acquire)
           ||p.channelEpoch.load(std::memory_order_acquire)!=publishedChannelEpochs[static_cast<std::size_t>(channel)].load(std::memory_order_acquire)
           ||p.visibility.load(std::memory_order_acquire)!=visibility)return result;
        if(auto* controls=currentControls.load(std::memory_order_acquire)){const auto& c=controls->slots[static_cast<std::size_t>(channel)][static_cast<std::size_t>(slot)];if(!c.enabled.load(std::memory_order_acquire))return result;}
        result.candidateHz=p.hz.load(std::memory_order_relaxed);result.modelScore=p.score.load(std::memory_order_relaxed);
        result.armed=p.armed.load(std::memory_order_relaxed);result.notchCount=p.count.load(std::memory_order_relaxed);
        for(int i=0;i<6;++i){result.notchHz[static_cast<std::size_t>(i)]=p.notchHz[static_cast<std::size_t>(i)].load(std::memory_order_relaxed);result.depthDb[static_cast<std::size_t>(i)]=p.depthDb[static_cast<std::size_t>(i)].load(std::memory_order_relaxed);}
        return visibility==meterVisibilityEpoch.load(std::memory_order_acquire)?result:FeedbackStatus{};
    }
private:
    struct MeterAccumulator {
        float inputPeak=0,outputPeak=0;
        std::uint64_t epoch=0,channelEpoch=0,visibility=0,generation=0;
        bool touched=false;
        std::array<float,3> reductionDb{};
        bool proDynamics=false;
    };
    struct PublishedMeter {
        std::atomic<std::uint64_t> peaks{0},epoch{0},channelEpoch{0},visibility{0},generation{0};
        std::atomic<std::uint64_t> sequence{0};
        std::array<std::atomic<float>,3> reductionDb{};
        std::atomic<bool> proDynamics{false};
    };
    void publishMeter(std::size_t index,int channel,std::uint64_t visibility)noexcept {
        const auto& meter=meterAccumulators[index];auto& published=metersPublished[index];
        published.sequence.fetch_add(1,std::memory_order_acq_rel);
        const bool valid=meter.touched && meter.epoch==globalEpoch
            &&meter.channelEpoch==channelEpochs[static_cast<std::size_t>(channel)]&&meter.visibility==visibility;
        const float input=valid?finite(meter.inputPeak):0,output=valid?finite(meter.outputPeak):0;
        static_assert(sizeof(float)==sizeof(std::uint32_t),"Meter packing requires 32-bit floats");
        std::uint32_t inputBits=0,outputBits=0;
        std::memcpy(&inputBits,&input,sizeof(float));std::memcpy(&outputBits,&output,sizeof(float));
        published.peaks.store(static_cast<std::uint64_t>(inputBits)|(static_cast<std::uint64_t>(outputBits)<<32),std::memory_order_release);
        published.generation.store(valid?meter.generation:0,std::memory_order_relaxed);
        for(int stage=0;stage<3;++stage)published.reductionDb[stage].store(valid?std::clamp(finite(meter.reductionDb[stage]),0.0f,120.0f):0.0f,std::memory_order_relaxed);
        published.proDynamics.store(valid&&meter.proDynamics,std::memory_order_relaxed);
        published.channelEpoch.store(channelEpochs[static_cast<std::size_t>(channel)],std::memory_order_release);
        published.visibility.store(visibility,std::memory_order_release);published.epoch.store(globalEpoch,std::memory_order_release);
        published.sequence.fetch_add(1,std::memory_order_release);
    }
    struct PublishedFeedback {
        std::atomic<float> hz{0},score{0};std::atomic<int> count{0};std::atomic<bool> armed{false};
        std::array<std::atomic<float>,6> notchHz{},depthDb{};
        std::atomic<std::uint64_t> epoch{0},channelEpoch{0},visibility{0};
    };
    void publishFeedback(std::size_t index,const FeedbackStatus& status,int channel)noexcept {
        auto& p=feedbackPublished[index];p.hz.store(status.candidateHz,std::memory_order_relaxed);p.score.store(status.modelScore,std::memory_order_relaxed);
        p.armed.store(status.armed,std::memory_order_relaxed);p.count.store(status.notchCount,std::memory_order_relaxed);
        for(int i=0;i<6;++i){p.notchHz[static_cast<std::size_t>(i)].store(status.notchHz[static_cast<std::size_t>(i)],std::memory_order_relaxed);p.depthDb[static_cast<std::size_t>(i)].store(status.depthDb[static_cast<std::size_t>(i)],std::memory_order_relaxed);}
        p.channelEpoch.store(channelEpochs[static_cast<std::size_t>(channel)],std::memory_order_release);
        p.visibility.store(callbackVisibilityEpoch,std::memory_order_release);p.epoch.store(globalEpoch,std::memory_order_release);
    }
    static float coefficient(double rate,float ms)noexcept{return static_cast<float>(std::exp(-1.0/(rate*.001*ms)));}
    static float envelope(float input,float& env,float attack,float release)noexcept {
        const float c=input>env?attack:release;env=c*env+(1-c)*input;
        if(env<1e-20f)env=0;return env;
    }
    static float compressed(float x,float env,float threshold,float ratio)noexcept {
        return env>threshold?x*std::pow(threshold/std::max(env,1e-12f),1-1/ratio):x;
    }
    static float saturate(float x,float drive)noexcept {
        return drive<=1?x:std::tanh(x*drive)/drive;
    }
    void configure(detail::State& s)noexcept;
    float process(detail::State& s,float x,int channel,int slot)noexcept;
    double fs=48000;
    bool prepared=false;
    spectrum::Capture* spectrumTap=nullptr;
    bool spectrumOutput=false;
    std::atomic<Controls*> currentControls{nullptr};
    std::unique_ptr<feedback::Processor[]> feedbackStates;
    std::array<std::uint32_t,channelCount*slotCount> feedbackRevision{};
    std::array<PublishedFeedback,channelCount*slotCount> feedbackPublished{};
    std::array<MeterAccumulator,channelCount*slotCount> meterAccumulators{};
    std::array<PublishedMeter,channelCount*slotCount> metersPublished{};
    std::atomic<std::uint64_t> meterVisibilityEpoch{1};
    std::uint64_t meterBlockNumber=0;
    std::uint64_t callbackVisibilityEpoch=0; // captured at beginBlock, never from an in-flight mute
    std::uint64_t globalEpoch=1;
    std::atomic<std::uint64_t> publishedEpoch {1};
    std::array<std::uint64_t,channelCount> channelEpochs {};
    std::array<std::atomic<std::uint64_t>,channelCount> publishedChannelEpochs {};
    std::array<std::array<detail::State,slotCount>,channelCount> states {};
    std::array<std::atomic<float>,channelCount*slotCount> candidates {};
    std::array<std::atomic<std::uint64_t>,channelCount*slotCount> candidateEpochs {},candidateChannelEpochs {},candidateVisibilityEpochs {};
};

inline void Rack::configure(detail::State& s)noexcept {
    const auto& p=s.p;
    auto hp=[&](int n,float frequency){s.filters[static_cast<std::size_t>(n)].filter(frequency,.70710678118655,0,fs,0);};
    auto lp=[&](int n,float frequency){s.filters[static_cast<std::size_t>(n)].filter(frequency,.70710678118655,0,fs,1);};
    auto bell=[&](int n,float frequency,float q,float db){s.filters[static_cast<std::size_t>(n)].filter(frequency,q,db,fs,2);};
    s.attack=coefficient(fs,10);s.release=coefficient(fs,120);s.slowRelease=coefficient(fs,500);
    switch(s.type) {
        case VocalClean:hp(0,p[0]);bell(1,250,.9f,p[1]);lp(2,5000);s.extraAttack=coefficient(fs,1);s.extraRelease=coefficient(fs,60);break;
        case VocalColor:hp(0,p[0]);bell(1,180,.7f,p[1]);bell(2,3500,.8f,p[2]);break;
        case FeedbackGuard:bell(0,p[0],p[2],p[1]);hp(1,p[3]);break;
        case AutoFeedback:bell(0,p[0],p[2],p[1]);s.release=coefficient(fs,100);break;
        case Equalizer:hp(0,p[0]);bell(1,100,.7f,p[1]);bell(2,p[2],p[4],p[3]);bell(3,8000,.7f,p[5]);break;
        case SmartEq:hp(0,p[0]);lp(1,400);lp(2,3000);bell(3,3000,.8f,p[2]);s.attack=s.release=coefficient(fs,p[4]);break;
        case Dynamics:case SmartComp:case OpticalComp:
            s.attack=coefficient(fs,p[2]);s.release=coefficient(fs,p[3]);s.slowRelease=coefficient(fs,p[3]*3);break;
        case Fx:
            s.lengths[0]=static_cast<std::size_t>(std::lround(fs*p[1]*.001));
            s.damping=static_cast<float>(std::exp(-2*pi*std::min(static_cast<double>(p[3]),fs*.45)/fs));break;
        case DigitalFx:s.lengths[0]=static_cast<std::size_t>(std::lround(fs*p[3]*.001));break;
        case Reverb96: {
            const double times[]={.0297,.0371,.0411,.0437,.005,.0017};
            std::size_t offset=0;
            for(int n=0;n<6;++n) {
                s.offsets[static_cast<std::size_t>(n)]=offset;
                s.lengths[static_cast<std::size_t>(n)]=std::max<std::size_t>(1,static_cast<std::size_t>(std::lround(fs*times[n]*(n<4?p[4]:1))));
                offset+=s.lengths[static_cast<std::size_t>(n)];
                if(n<4)s.combFeedback[static_cast<std::size_t>(n)]=static_cast<float>(std::pow(10.0,-3*static_cast<double>(s.lengths[static_cast<std::size_t>(n)])/(fs*p[1])));
            }
            s.offsets[6]=offset;s.lengths[6]=std::max<std::size_t>(1,static_cast<std::size_t>(std::lround(fs*p[3]*.001)));
            for(int n=0;n<7;++n)if(s.positions[static_cast<std::size_t>(n)]>=s.lengths[static_cast<std::size_t>(n)])s.positions[static_cast<std::size_t>(n)]=0;
            s.damping=static_cast<float>(std::exp(-2*pi*std::min(static_cast<double>(p[2]),fs*.45)/fs));break;
        }
        case SubPro:hp(0,p[0]);lp(1,p[1]);lp(2,p[1]);bell(3,55,.8f,p[2]);break;
        case DrumLive:hp(0,p[0]);bell(1,90,.8f,p[1]);bell(2,4500,.8f,p[2]);s.attack=coefficient(fs,1);s.release=coefficient(fs,p[4]);s.slowRelease=coefficient(fs,30);break;
        case PcAudio:hp(0,p[0]);bell(1,100,.7f,p[1]);bell(2,7000,.7f,p[2]);break;
        case Analog:hp(0,p[0]);bell(1,150,.6f,p[1]);bell(2,10000,.6f,p[2]);break;
        case FourKLine:hp(0,p[0]);bell(1,100,.7f,p[1]);bell(2,400,1.1f,p[2]);bell(3,3000,1.1f,p[3]);bell(4,10000,.7f,p[4]);break;
        default:break;
    }
}

inline float Rack::process(detail::State& s,float x,int channel,int slot)noexcept {
    const auto& p=s.p;
    switch(s.type) {
        case VocalClean: {
            float y=s.filters[1].tick(s.filters[0].tick(x));
            const float low=s.filters[2].tick(y),high=y-low;
            const float hf=envelope(std::abs(high),s.env[1],s.extraAttack,s.extraRelease);
            y=low+high/(1+p[2]*10*std::max(0.0f,hf-.06f));
            return compressed(y,envelope(std::abs(y),s.env[0],s.attack,s.release),s.gains[3],p[4])*s.gains[5];
        }
        case VocalColor: {
            const float y=saturate(s.filters[2].tick(s.filters[1].tick(s.filters[0].tick(x))),s.gains[3]);
            return (x+(y-x)*p[4]*.01f)*s.gains[5];
        }
        case FeedbackGuard: {
            const float y=s.filters[1].tick(s.filters[0].tick(x)),ceiling=s.gains[4];
            return x+(std::clamp(y,-ceiling,ceiling)-x)*p[5]*.01f;
        }
        case AutoFeedback: {
            ++s.crossSamples;
            if(s.previousInput<=0&&x>0) {
                const float interval=static_cast<float>(s.crossSamples);
                const bool valid=interval>=fs/8000.0&&interval<=fs/80.0;
                if(valid&&(s.period==0||std::abs(interval-s.period)<=std::max(1.0f,s.period*p[4]*.01f))) {
                    s.stableSamples+=interval;s.period=s.period==0?interval:.9f*s.period+.1f*interval;
                }else{s.stableSamples=0;s.period=valid?interval:0;}
                s.crossSamples=0;
            }
            const float level=envelope(std::abs(x),s.env[0],s.attack,s.release);
            if(s.crossSamples>fs*.1||level<.005f){s.stableSamples=0;s.period=0;}
            const float candidate=s.stableSamples>=fs*p[3]*.001&&s.period>0?static_cast<float>(fs/s.period):0.0f;
            if(candidate!=s.lastCandidate) {
                const auto index=static_cast<std::size_t>(channel*slotCount+slot);
                candidates[index].store(candidate,std::memory_order_relaxed);
                candidateEpochs[index].store(globalEpoch,std::memory_order_release);
                candidateChannelEpochs[index].store(channelEpochs[static_cast<std::size_t>(channel)],std::memory_order_release);
                candidateVisibilityEpochs[index].store(callbackVisibilityEpoch,std::memory_order_release);
                s.lastCandidate=candidate;
            }
            return s.filters[0].tick(x)*s.gains[5];
        }
        case Equalizer:{
            if(s.extendedEq){for(std::size_t band=0;band<s.eqFilters.size();++band)if(s.eqSettings[band].enabled)x=s.eqFilters[band].tick(x);return x;}
            return s.filters[3].tick(s.filters[2].tick(s.filters[1].tick(s.filters[0].tick(x))));
        }
        case SmartEq: {
            const float y=s.filters[0].tick(x),low=s.filters[1].tick(y),belowHigh=s.filters[2].tick(y),high=y-belowHigh;
            const float all=envelope(std::abs(y),s.env[0],s.attack,s.release)+1e-6f;
            const float l=envelope(std::abs(low),s.env[1],s.attack,s.release)/all;
            const float h=envelope(std::abs(high),s.env[2],s.attack,s.release)/all;
            const float lowGain=1/(1+p[1]*std::max(0.0f,l-.55f));
            const float highGain=1/(1+p[3]*std::max(0.0f,h-.45f));
            return s.filters[3].tick(low*lowGain+(belowHigh-low)+high*highGain)*s.gains[5];
        }
        case Dynamics: {
            if(s.dynamicsSettings.enabled)return s.dynamicsProcessor.tick(x);
            const float env=envelope(std::abs(x),s.env[0],s.attack,s.release),gate=s.gains[4];
            const float expansion=env<gate?std::clamp(env/std::max(gate,1e-8f),0.0f,1.0f):1;
            return compressed(x,env,s.gains[0],p[1])*expansion*s.gains[5];
        }
        case SmartComp: {
            const float release=s.release+(s.slowRelease-s.release)*std::clamp(s.env[0]*4,0.0f,1.0f);
            const float rms=std::sqrt(envelope(x*x,s.env[0],s.attack,release));
            const float y=compressed(x,rms,s.gains[0],p[1]);return (x+(y-x)*p[4]*.01f)*s.gains[5];
        }
        case OpticalComp: {
            const float y=saturate(x,s.gains[4]);
            const float fast=envelope(std::abs(y),s.env[0],s.attack,s.release);
            const float slow=envelope(std::abs(y),s.env[1],s.attack,s.slowRelease);
            return compressed(y,std::max(fast,slow),s.gains[0],p[1])*s.gains[5];
        }
        case Fx: {
            const auto size=s.delay.size(),read=s.position>=s.lengths[0]?s.position-s.lengths[0]:s.position+size-s.lengths[0];
            const float wet=s.read(read);
            s.env[0]=s.damping*s.env[0]+(1-s.damping)*wet;
            s.write(s.position,saturate(x+s.env[0]*p[2],s.gains[4]));if(++s.position==size)s.position=0;
            return (x+(wet-x)*p[0]*.01f)*s.gains[5];
        }
        case DigitalFx: {
            if(s.digitalFxSettings.enabled)return s.digitalFxProcessor.tick(x,s);
            if(s.holdCounter==0) {
                const float scale=std::ldexp(1.0f,static_cast<int>(p[0])-1);
                s.held=std::round(std::clamp(x,-1.0f,1.0f)*scale)/scale;
            }
            s.holdCounter=(s.holdCounter+1)%static_cast<unsigned>(std::max(1.0f,std::round(p[1])));
            const auto size=s.delay.size(),read=s.position>=s.lengths[0]?s.position-s.lengths[0]:s.position+size-s.lengths[0];
            const float echo=s.read(read);s.write(s.position,s.held+echo*p[4]);if(++s.position==size)s.position=0;
            const float wet=.65f*s.held+.35f*echo;return (x+(wet-x)*p[2]*.01f)*s.gains[5];
        }
        case Reverb96: {
            float input=x;
            if(p[3]>0) {
                const auto index=s.offsets[6]+s.positions[6];input=s.read(index);s.write(index,x);
                if(++s.positions[6]==s.lengths[6])s.positions[6]=0;
            }
            float wet=0;
            for(int n=0;n<4;++n) {
                const auto index=s.offsets[static_cast<std::size_t>(n)]+s.positions[static_cast<std::size_t>(n)];
                const float delayed=s.read(index);
                auto& damp=s.env[static_cast<std::size_t>(n)];damp=s.damping*damp+(1-s.damping)*delayed;
                s.write(index,input+damp*s.combFeedback[static_cast<std::size_t>(n)]);wet+=delayed*.25f;
                if(++s.positions[static_cast<std::size_t>(n)]==s.lengths[static_cast<std::size_t>(n)])s.positions[static_cast<std::size_t>(n)]=0;
            }
            for(int n=4;n<6;++n) {
                const auto index=s.offsets[static_cast<std::size_t>(n)]+s.positions[static_cast<std::size_t>(n)];
                const float delayed=s.read(index),out=-.5f*wet+delayed;s.write(index,wet+.5f*out);wet=out;
                if(++s.positions[static_cast<std::size_t>(n)]==s.lengths[static_cast<std::size_t>(n)])s.positions[static_cast<std::size_t>(n)]=0;
            }
            return (x+(wet-x)*p[0]*.01f)*s.gains[5];
        }
        case SubPro: {
            const float y=s.filters[3].tick(s.filters[2].tick(s.filters[1].tick(s.filters[0].tick(x))));
            return saturate(y,s.gains[4])*(p[3]>=.5f?-1.0f:1.0f)*s.gains[5];
        }
        case DrumLive: {
            const float y=s.filters[2].tick(s.filters[1].tick(s.filters[0].tick(x))),a=std::abs(y);
            const float fast=envelope(a,s.env[0],s.attack,s.release),slow=envelope(a,s.env[1],s.slowRelease,s.release);
            const float transient=std::clamp((fast-slow)/(fast+1e-7f),0.0f,1.0f);
            const float gate=std::clamp(fast/std::max(s.gains[3],1e-8f),0.0f,1.0f);
            return y*gate*(1+(s.gains[5]-1)*transient);
        }
        case PcAudio: {
            const float y=s.filters[2].tick(s.filters[1].tick(s.filters[0].tick(x)));
            const float value=compressed(y,envelope(std::abs(y),s.env[0],s.attack,s.release),s.gains[3],p[4]);
            const float ceiling=s.gains[5];return std::clamp(value,-ceiling,ceiling);
        }
        case Analog: {
            const float shaped=s.filters[2].tick(s.filters[1].tick(s.filters[0].tick(x)));
            const float wet=saturate(shaped,s.gains[3]);
            return (x+(wet-x)*p[4]*.01f)*s.gains[5];
        }
        case FourKLine: {
            float shaped=x;
            for(auto& filter:s.filters)shaped=filter.tick(shaped);
            return saturate(shaped,s.gains[5]);
        }
        default:return x;
    }
}
} // namespace lah::builtin

