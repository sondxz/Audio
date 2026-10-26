#pragma once
// No JUCE dependency. Every allocation takes place in prepare(), never in process().
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>
#include "BuiltinRackCore.h"
#include "BusMasterCore.h"

namespace lah::dsp {
constexpr int channels = 32;
constexpr double pi = 3.14159265358979323846;
inline float clean(float v) noexcept { return std::isfinite(v) ? v : 0.0f; }
inline float clamp(float x,float lo,float hi) noexcept { return std::clamp(clean(x),lo,hi); }
inline float dB(float x) noexcept {return std::pow(10.0f,clamp(x,-100.0f,24.0f)/20.0f);}

struct Controls {
    BusMasterControls busMaster;
    // GUI writes atomics, audio thread takes a per-block snapshot.
    std::array<std::atomic<float>,channels> inputGain{}, eqDb{}, eqHz{}, eqQ{}, hpfHz{},
        compThreshold{}, compRatio{}, compAttackMs{}, compReleaseMs{}, gateThreshold{},
        outputGain{}, outputDelayMs{}, outputPolarity{}, outputCeiling{};
    std::array<std::atomic<float>,channels> fxSend{}, inputMute{}, inputPan{};
    // INPUT PAN is balance-style only for matrix OUT 1/2. Default 0 preserves unity gain.
    std::atomic<float> delayReturn {0},reverbReturn {0},fxDelayMs {250},fxFeedback {0.3f};
    std::array<std::atomic<float>,channels*channels> route{}; // [out * 32 + in]
    std::atomic<int> referenceChannel {0}, measurementChannel {1};
    Controls() {
        for(int ch=0; ch<channels; ++ch) {
            inputGain[ch].store(0); inputMute[ch].store(0); inputPan[ch].store(0); eqDb[ch].store(0); eqHz[ch].store(1000);fxSend[ch].store(0);
            eqQ[ch].store(1); hpfHz[ch].store(0); compThreshold[ch].store(0);
            compRatio[ch].store(1); compAttackMs[ch].store(10); compReleaseMs[ch].store(100);
            gateThreshold[ch].store(-100); outputGain[ch].store(0); outputDelayMs[ch].store(0);
            outputPolarity[ch].store(1); outputCeiling[ch].store(0);
        }
        for(int o=0;o<channels;++o) for(int i=0;i<channels;++i)
            route[o*channels+i].store(o==i?1.0f:0.0f);
    }
};

struct Biquad {
    double b0=1,b1=0,b2=0,a1=0,a2=0,z1=0,z2=0;
    void reset() noexcept { z1=z2=0; }
    void configure(double b0_,double b1_,double b2_,double a0_,double a1_,double a2_) noexcept {
        if(a0_ <= 1e-15) return;
        b0=b0_/a0_; b1=b1_/a0_; b2=b2_/a0_; a1=a1_/a0_; a2=a2_/a0_;
    }
    void peak(double hz,double q,double db,double sr) noexcept {
        if(sr<=0) return;
        const double w=2*pi*std::clamp(hz,20.0,sr*0.45)/sr;
        const double alpha=std::sin(w)/(2*std::clamp(q,0.1,10.0));
        const double A=std::pow(10.0,std::clamp(db,-18.0,18.0)/40.0);
        configure(1+alpha*A,-2*std::cos(w),1-alpha*A,1+alpha/A,-2*std::cos(w),1-alpha/A);
    }
    void highpass(double hz,double sr) noexcept {
        if(hz<10.0 || sr<=0){b0=1;b1=b2=a1=a2=0;return;}
        const double w=2*pi*std::clamp(hz,10.0,sr*0.45)/sr;
        const double alpha=std::sin(w)/(2*0.70710678118655);
        const double c=std::cos(w);
        configure((1+c)/2,-(1+c),(1+c)/2,1+alpha,-2*c,1-alpha);
    }
    float tick(float x) noexcept {
        const double y=b0*x+z1;
        z1=b1*x-a1*y+z2; z2=b2*x-a2*y;
        if(!std::isfinite(y) || !std::isfinite(z1) || !std::isfinite(z2)) {reset(); return 0;}
        if(std::abs(z1)<1e-30) z1=0;
        if(std::abs(z2)<1e-30) z2=0;
        return clean(static_cast<float>(y));
    }
};


class StereoEffects {
public:
    struct Wet {float delayL=0,delayR=0,reverbL=0,reverbR=0;};
    void prepare(double rate) {
        if (!std::isfinite(rate)) throw std::invalid_argument("StereoEffects sample rate must be finite");
        sr=std::clamp(rate,8000.0,192000.0);
        auto length=static_cast<std::size_t>(std::ceil(sr*2.0))+2;
        echoL.assign(length,0);echoR.assign(length,0);echoPos=0;
        echoStampsL.assign(length,0);echoStampsR.assign(length,0);epoch=1;
        const double times[4]={0.0297,0.0371,0.0411,0.0437};
        for(int k=0;k<4;++k){
            combL[k].assign(static_cast<std::size_t>(std::ceil(sr*times[k]))+1,0);
            combR[k].assign(static_cast<std::size_t>(std::ceil(sr*(times[k]+0.0017)))+1,0);
            combStampsL[k].assign(combL[k].size(),0);
            combStampsR[k].assign(combR[k].size(),0);
            combPosL[k]=combPosR[k]=0;
        }
    }
    // O(1) invalidation discards all old FX samples without clearing megabytes
    // of delay buffers in the audio callback. Epoch overflow wraps only after
    // 2^64 mute cycles (not a practical runtime condition).
    void invalidate() noexcept {++epoch;if(epoch==0)++epoch;}
    Wet tick(float send,float ms,float feedback) noexcept {
        if(echoL.empty()) return {};
        const auto lag=static_cast<std::size_t>(std::clamp(sr*0.001*clamp(ms,10,1800),1.0,static_cast<double>(echoL.size()-2)));
        const auto size=echoL.size();
        const auto read=(echoPos+size-lag)%size;
        const float dl=echoStampsL[read]==epoch?echoL[read]:0.0f;
        const float dr=echoStampsR[read]==epoch?echoR[read]:0.0f;
        const float fb=clamp(feedback,0,0.85f);
        echoL[echoPos]=clamp(send+dr*fb,-2,2);
        echoR[echoPos]=clamp(dl*fb,-2,2);
        echoStampsL[echoPos]=echoStampsR[echoPos]=epoch;
        echoPos=(echoPos+1)%size;
        float left=0,right=0;
        for(int k=0;k<4;++k){
            auto& lp=combPosL[k];auto& rp=combPosR[k];
            const float l=combStampsL[k][lp]==epoch?combL[k][lp]:0.0f;
            const float r=combStampsR[k][rp]==epoch?combR[k][rp]:0.0f;
            combL[k][lp]=clamp(send+0.72f*l,-2,2);
            combR[k][rp]=clamp(send+0.72f*r,-2,2);
            combStampsL[k][lp]=combStampsR[k][rp]=epoch;
            lp=(lp+1)%combL[k].size();rp=(rp+1)%combR[k].size();
            left+=l*0.2f;right+=r*0.2f;
        }
        return {dl,dr,left,right};
    }
private:
    double sr=48000;
    std::vector<float> echoL,echoR;
    std::vector<std::uint64_t> echoStampsL,echoStampsR;
    std::size_t echoPos=0;
    std::uint64_t epoch=1;
    std::array<std::vector<float>,4> combL{},combR{};
    std::array<std::vector<std::uint64_t>,4> combStampsL{},combStampsR{};
    std::array<std::size_t,4> combPosL{},combPosR{};
};

class Mixer {
public:
    explicit Mixer(Controls& p,builtin::Rack* rack=nullptr,builtin::Controls* rackControls=nullptr,
                   builtin::Rack* outputRack=nullptr,builtin::Controls* outputRackControls=nullptr)
        :ctrl(p),nativeRack(rack),nativeControls(rackControls),
         outputNativeRack(outputRack),outputNativeControls(outputRackControls) {}
    void prepare(double sr) {
        if (!std::isfinite(sr)) throw std::invalid_argument("Mixer sample rate must be finite");
        rate=std::clamp(sr,8000.0,192000.0);
        // Reserve 250ms maximum output alignment delay, for up to 32 output channels.
        const auto needed=static_cast<std::size_t>(std::ceil(rate*0.25))+2;
        for(auto& d: delays) d.assign(needed,0.0f);
        fx.prepare(rate);
        if(nativeRack)nativeRack->prepare(rate,512);
        if(outputNativeRack)outputNativeRack->prepare(rate,512);
        nativeMuted.fill(false);
        for(auto& f: eq) f.reset();
        for(auto& f: hpf) f.reset();
        envelopes.fill(0); positions.fill(0); activeFrames.fill(0); rampsInitialized=false;wasEnabled=false;
        busMaster.reset();
    }
    float consumeBusMasterPeak(int index)noexcept{return busMaster.consumePeak(index);}
    void invalidateBusMasterMeters()noexcept{busMaster.invalidateMeters();}
    // Inputs and outputs are callback-slot mappings, not physical channel numbers.
    void process(const float* const* inputs,int ni,float* const* outputs,int no,int n,bool enabled,
                 float* inPeaks=nullptr,float* outPeaks=nullptr,int peakCapacity=0) noexcept {
        ni=std::clamp(ni,0,channels); no=std::clamp(no,0,channels); n=std::max(0,n);
        if(inPeaks) std::fill_n(inPeaks,std::max(0,peakCapacity),0.0f);
        if(outPeaks) std::fill_n(outPeaks,std::max(0,peakCapacity),0.0f);
        if(outputs==nullptr) return;
        // No input passthrough until explicit user confirmation. No stale tails after mute.
        if(!enabled || delays[0].empty()) {
            if(!enabled) {
                activeFrames.fill(0);
                if (wasEnabled) {
                    fx.invalidate();
                    busMaster.reset();
                    if(nativeRack)nativeRack->reset();
                    if(outputNativeRack)outputNativeRack->reset();
                    for(auto& f:eq)f.reset();
                    for(auto& f:hpf)f.reset();
                    envelopes.fill(0);
                    rampsInitialized=false;
                    wasEnabled=false;
                }
            }
            for(int o=0;o<no;++o) if(outputs[o]) std::fill_n(outputs[o],n,0.0f);
            // Still observe input peak levels even while muted.
            for(int i=0;i<std::min(ni,peakCapacity);++i) if(inPeaks && inputs && inputs[i])
                for(int s=0;s<n;++s) inPeaks[i]=std::max(inPeaks[i],std::abs(clean(inputs[i][s])));
            return;
        }
        wasEnabled=true;
        if(nativeRack && nativeControls)nativeRack->beginBlock(*nativeControls);
        if(outputNativeRack && outputNativeControls)outputNativeRack->beginBlock(*outputNativeControls);
        busMaster.beginBlock(ctrl.busMaster);
        for(int i=0;i<ni;++i) {
            const bool muted=ctrl.inputMute[i].load(std::memory_order_relaxed)>0.5f;
            if(nativeRack && muted!=nativeMuted[static_cast<std::size_t>(i)])nativeRack->resetChannel(i);
            nativeMuted[static_cast<std::size_t>(i)]=muted;
            gainNow[i]=ctrl.inputMute[i].load(std::memory_order_relaxed)>0.5f ? 0.0f :
                     dB(ctrl.inputGain[i].load(std::memory_order_relaxed));
            const auto balance=clamp(ctrl.inputPan[i].load(std::memory_order_relaxed),-1.0f,1.0f);
            leftBalance[i]=1.0f-std::max(0.0f,balance);
            rightBalance[i]=1.0f+std::min(0.0f,balance);
            sendGain[i]=clamp(ctrl.fxSend[i].load(),0.0f,1.0f);
            eq[i].peak(ctrl.eqHz[i].load(),ctrl.eqQ[i].load(),ctrl.eqDb[i].load(),rate);
            hpf[i].highpass(ctrl.hpfHz[i].load(),rate);
            threshold[i]=dB(ctrl.compThreshold[i].load());
            ratio[i]=std::clamp(ctrl.compRatio[i].load(),1.0f,20.0f);
            gate[i]=dB(ctrl.gateThreshold[i].load());
            attack[i]=std::exp(-1.0f/static_cast<float>(rate*0.001*std::max(0.2f,ctrl.compAttackMs[i].load())));
            release[i]=std::exp(-1.0f/static_cast<float>(rate*0.001*std::max(1.0f,ctrl.compReleaseMs[i].load())));
        }
        for(int o=0;o<no;++o) {
            outGain[o]=dB(ctrl.outputGain[o].load());
            polarity[o]=ctrl.outputPolarity[o].load()<0 ? -1.0f:1.0f;
            ceiling[o]=dB(std::clamp(ctrl.outputCeiling[o].load(),-24.0f,0.0f));
            delaySamples[o]=std::clamp(static_cast<int>(std::lround(rate*0.001*clamp(ctrl.outputDelayMs[o].load(),0.0f,250.0f))),0,static_cast<int>(delays[o].size())-2);
            activeCount[o]=0;
            for(int i=0;i<ni;++i) {
                auto idx=o*channels+i;
                routeTarget[idx]=clamp(ctrl.route[idx].load(std::memory_order_relaxed),-2.0f,2.0f);
                if(!rampsInitialized) routePrevious[idx]=routeTarget[idx];
                if(std::abs(routePrevious[idx])>0.000001f || std::abs(routeTarget[idx])>0.000001f)
                    activeInputs[o][activeCount[o]++]=i;
            }
        }
        const float wetDelay=clamp(ctrl.delayReturn.load(),0.0f,1.0f);
        const float wetReverb=clamp(ctrl.reverbReturn.load(),0.0f,1.0f);
        const float fxMs=clamp(ctrl.fxDelayMs.load(),10.0f,1800.0f);
        const float fxFeedback=clamp(ctrl.fxFeedback.load(),0,0.85f);
        if(!rampsInitialized) for(int i=0;i<ni;++i) gainPrevious[i]=gainNow[i];
        rampsInitialized=true;
        for(int s=0;s<n;++s) {
            const auto t=static_cast<float>(s+1)/static_cast<float>(std::max(1,n));
            for(int i=0;i<ni;++i) {
                const float x=(inputs && inputs[i])?clean(inputs[i][s]):0.0f;
                if(inPeaks && i<peakCapacity) inPeaks[i]=std::max(inPeaks[i],std::abs(x));
                float v=hpf[i].tick(x*(gainPrevious[i]+(gainNow[i]-gainPrevious[i])*t));
                v=eq[i].tick(v);
                const float a=std::abs(v);
                const float coeff=a>envelopes[i]?attack[i]:release[i];
                envelopes[i]=coeff*envelopes[i]+(1.0f-coeff)*a;
                const float env=envelopes[i];
                // Basic downward expander gate and feed-forward compressor (soft safety damping).
                float reduction=1.0f;
                if(env<gate[i] && gate[i]>0.0001f) reduction=std::clamp(env/gate[i],0.0f,1.0f);
                if(env>threshold[i] && threshold[i]>0.0f && ratio[i]>1.0f)
                    reduction*=std::pow(threshold[i]/std::max(env,1e-12f),1.0f-1.0f/ratio[i]);
                processed[i]=clean(v*reduction);
                if(nativeRack){
                    processed[i]=nativeRack->processSample(i,processed[i]);
                    if(nativeMuted[static_cast<std::size_t>(i)])processed[i]=0.0f;
                }
            }
            float sumFx=0;
            for(int i=0;i<ni;++i)sumFx+=processed[i]*sendGain[i];
            const auto wet=fx.tick(std::clamp(sumFx,-2.0f,2.0f),fxMs,fxFeedback);
            std::array<float,channels> busReturns{};
            busMaster.processSample(processed.data(),ni,t,busReturns);
            for(int o=0;o<no;++o) {
                float v=(o==0 ? wet.delayL*wetDelay+wet.reverbL*wetReverb
                           : o==1 ? wet.delayR*wetDelay+wet.reverbR*wetReverb : 0.0f);
                v+=busReturns[o];
                for(int k=0;k<activeCount[o];++k) {
                    const int i=activeInputs[o][k], idx=o*channels+i;
                    const float r=routePrevious[idx]+(routeTarget[idx]-routePrevious[idx])*t;
                    const auto panWeight=o==0?leftBalance[i]:o==1?rightBalance[i]:1.0f;
                    v+=processed[i]*r*panWeight;
                }
                // Each USB return has its own ordered native insert rack. Process
                // the complete matrix/FX sum before the output level and guard.
                if(outputNativeRack)v=outputNativeRack->processSample(o,v);
                v=clean(v*outGain[o]*polarity[o]);
                auto& ring=delays[o]; auto& pos=positions[o];
                ring[pos]=v;
                const auto length=ring.size();
                const auto lag=static_cast<std::size_t>(delaySamples[o]);
                const float delayed=activeFrames[o] >= static_cast<std::size_t>(lag)
                     ? ring[(pos+length-lag)%length] : 0.0f;
                if(activeFrames[o]<length) ++activeFrames[o];
                pos=(pos+1)%length;
                // A hard clip guard, NOT a brick-wall true-peak limiter (no look-ahead).
                const float result=std::clamp(clean(delayed),-ceiling[o],ceiling[o]);
                if(outputs[o]) outputs[o][s]=result;
                if(outPeaks && o<peakCapacity) outPeaks[o]=std::max(outPeaks[o],std::abs(result));
            }
        }
        for(int i=0;i<ni;++i) gainPrevious[i]=gainNow[i];
        busMaster.endBlock();
        for(int o=0;o<no;++o) for(int i=0;i<ni;++i) routePrevious[o*channels+i]=routeTarget[o*channels+i];
    }
private:
    Controls& ctrl;
    StereoEffects fx;
    BusMasterProcessor busMaster;
    double rate=48000;
    std::array<Biquad,channels> eq{},hpf{};
    builtin::Rack* nativeRack=nullptr;
    builtin::Controls* nativeControls=nullptr;
    builtin::Rack* outputNativeRack=nullptr;
    builtin::Controls* outputNativeControls=nullptr;
    std::array<bool,channels> nativeMuted{};
    std::array<float,channels> leftBalance{},rightBalance{},sendGain{},envelopes{},gainNow{},gainPrevious{},threshold{},ratio{},gate{},attack{},release{},processed{},outGain{},polarity{},ceiling{};
    std::array<int,channels> delaySamples{},activeCount{};
    std::array<std::array<int,channels>,channels> activeInputs{};
    std::array<std::vector<float>,channels> delays{};
    std::array<std::size_t,channels> positions{}, activeFrames{};
    std::array<float,channels*channels> routeTarget{},routePrevious{};
    bool rampsInitialized=false,wasEnabled=false;
};
} // namespace lah::dsp
