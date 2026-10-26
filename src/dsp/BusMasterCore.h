#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace lah::dsp {
constexpr int softwareBusCount=8, busSourceCount=32;
struct BusMasterSnapshot {
    bool enabled=false,masterMute=false;
    std::array<float,softwareBusCount*busSourceCount> sends{};
    std::array<float,busSourceCount> masterSends{},inputPan{};
    std::array<float,softwareBusCount> gainDb{},mute{},pan{},toMaster{};
    std::array<int,softwareBusCount> output{};
    std::array<float,2> masterGainDb{};
    int masterLeft=0,masterRight=1;
    BusMasterSnapshot(){output.fill(-1);toMaster.fill(1);}
};
inline float busBound(float value,float lo,float hi,float fallback=0)noexcept{
    return std::isfinite(value)?std::clamp(value,lo,hi):fallback;
}
inline BusMasterSnapshot boundedBusMaster(BusMasterSnapshot value)noexcept{
    for(auto& v:value.sends)v=busBound(v,0,2);
    for(auto& v:value.masterSends)v=busBound(v,0,2);
    for(auto& v:value.inputPan)v=busBound(v,-1,1);
    for(auto& v:value.gainDb)v=busBound(v,-60,12);
    for(auto& v:value.mute)v=busBound(v,0,1);
    for(auto& v:value.pan)v=busBound(v,-1,1);
    for(auto& v:value.toMaster)v=busBound(v,0,2,1);
    for(auto& v:value.output)if(v< -1||v>=busSourceCount)v=-1;
    for(auto& v:value.masterGainDb)v=busBound(v,-60,12);
    if(value.masterLeft< -1||value.masterLeft>=busSourceCount)value.masterLeft=-1;
    if(value.masterRight< -1||value.masterRight>=busSourceCount)value.masterRight=-1;
    if(value.masterLeft>=0&&value.masterLeft==value.masterRight)value.masterRight=-1;
    return value;
}
struct BusMasterControls {
    std::atomic<bool> enabled{false},masterMute{false};
    std::array<std::atomic<float>,softwareBusCount*busSourceCount> sends{};
    std::array<std::atomic<float>,busSourceCount> masterSends{},inputPan{};
    std::array<std::atomic<float>,softwareBusCount> gainDb{},mute{},pan{},toMaster{};
    std::array<std::atomic<int>,softwareBusCount> output{};
    std::array<std::atomic<float>,2> masterGainDb{};
    std::atomic<int> masterLeft{0},masterRight{1};
    BusMasterControls(){for(auto& v:output)v.store(-1);for(auto& v:toMaster)v.store(1);}
};
inline BusMasterSnapshot snapshotBusMaster(const BusMasterControls& source)noexcept{
    BusMasterSnapshot result;
    result.enabled=source.enabled.load(std::memory_order_acquire);result.masterMute=source.masterMute.load();
    for(std::size_t i=0;i<result.sends.size();++i)result.sends[i]=source.sends[i].load();
    for(std::size_t i=0;i<result.masterSends.size();++i){result.masterSends[i]=source.masterSends[i].load();result.inputPan[i]=source.inputPan[i].load();}
    for(std::size_t i=0;i<result.gainDb.size();++i){
        result.gainDb[i]=source.gainDb[i].load();result.mute[i]=source.mute[i].load();
        result.pan[i]=source.pan[i].load();result.toMaster[i]=source.toMaster[i].load();result.output[i]=source.output[i].load();
    }
    for(int i=0;i<2;++i)result.masterGainDb[i]=source.masterGainDb[i].load();
    result.masterLeft=source.masterLeft.load();result.masterRight=source.masterRight.load();
    return boundedBusMaster(result);
}
// Control path; caller mutes transport before a complete recall/routing change.
inline void recallBusMaster(BusMasterControls& target,const BusMasterSnapshot& raw)noexcept{
    const auto source=boundedBusMaster(raw);target.enabled.store(false,std::memory_order_release);
    for(std::size_t i=0;i<source.sends.size();++i)target.sends[i].store(source.sends[i]);
    for(std::size_t i=0;i<source.masterSends.size();++i){target.masterSends[i].store(source.masterSends[i]);target.inputPan[i].store(source.inputPan[i]);}
    for(std::size_t i=0;i<source.gainDb.size();++i){target.gainDb[i].store(source.gainDb[i]);target.mute[i].store(source.mute[i]);target.pan[i].store(source.pan[i]);target.toMaster[i].store(source.toMaster[i]);target.output[i].store(source.output[i]);}
    for(int i=0;i<2;++i)target.masterGainDb[i].store(source.masterGainDb[i]);
    target.masterLeft.store(source.masterLeft);target.masterRight.store(source.masterRight);target.masterMute.store(source.masterMute);
    target.enabled.store(source.enabled,std::memory_order_release);
}

// Eight mono software buses and a true stereo sum. Sources are post input DSP,
// physical returns are added before each output's native rack/gain/delay/guard.
// This processor has no delays, allocations, locks or device dependencies.
class BusMasterProcessor {
public:
    void reset()noexcept{initialised=false;invalidateMeters();}
    void invalidateMeters()noexcept{visibility.fetch_add(1,std::memory_order_release);}
    void beginBlock(const BusMasterControls& control)noexcept{
        state=snapshotBusMaster(control);target={};
        target.sends=state.sends;target.masterSends=state.masterSends;
        target.toMaster=state.toMaster;target.enabled=state.enabled?1.0f:0.0f;
        for(int i=0;i<busSourceCount;++i){const float angle=(state.inputPan[i]+1)*quarterPi;target.inputL[i]=std::cos(angle);target.inputR[i]=std::sin(angle);}
        for(int b=0;b<softwareBusCount;++b){
            target.gain[b]=state.mute[b]>.5f?0:linear(state.gainDb[b]);
            const float angle=(state.pan[b]+1)*quarterPi;target.busL[b]=std::cos(angle);target.busR[b]=std::sin(angle);
        }
        for(int lr=0;lr<2;++lr)target.masterGain[lr]=state.masterMute?0:linear(state.masterGainDb[lr]);
        if(!initialised){previous=target;initialised=true;}
        blockPeaks.fill(0);blockVisibility=visibility.load(std::memory_order_acquire);
    }
    void processSample(const float* input,int count,float interpolation,std::array<float,busSourceCount>& returns)noexcept{
        returns.fill(0);const float t=std::clamp(interpolation,0.0f,1.0f);
        const float active=mix(previous.enabled,target.enabled,t);
        if(active==0)return;
        count=std::clamp(count,0,busSourceCount);
        float left=0,right=0;
        for(int i=0;i<count;++i){
            const float x=input&&std::isfinite(input[i])?input[i]:0;
            const float send=mix(previous.masterSends[i],target.masterSends[i],t);
            left+=x*send*mix(previous.inputL[i],target.inputL[i],t);
            right+=x*send*mix(previous.inputR[i],target.inputR[i],t);
        }
        for(int b=0;b<softwareBusCount;++b){
            float sum=0;
            for(int i=0;i<count;++i){
                const float x=input&&std::isfinite(input[i])?input[i]:0;
                const int index=b*busSourceCount+i;
                sum+=x*mix(previous.sends[index],target.sends[index],t);
            }
            const float bus=finiteSignal(sum*mix(previous.gain[b],target.gain[b],t));
            const float send=mix(previous.toMaster[b],target.toMaster[b],t);
            left+=bus*send*mix(previous.busL[b],target.busL[b],t);
            right+=bus*send*mix(previous.busR[b],target.busR[b],t);
            const float physical=bus*active;blockPeaks[b]=std::max(blockPeaks[b],std::abs(physical));
            if(state.output[b]>=0)returns[state.output[b]]+=physical;
        }
        left=finiteSignal(left*mix(previous.masterGain[0],target.masterGain[0],t)*active);
        right=finiteSignal(right*mix(previous.masterGain[1],target.masterGain[1],t)*active);
        blockPeaks[8]=std::max(blockPeaks[8],std::abs(left));blockPeaks[9]=std::max(blockPeaks[9],std::abs(right));
        if(state.masterLeft>=0)returns[state.masterLeft]+=left;
        if(state.masterRight>=0)returns[state.masterRight]+=right;
        for(auto& value:returns)value=finiteSignal(value);
    }
    void endBlock()noexcept{
        previous=target;
        for(std::size_t i=0;i<peaks.size();++i){
            if(peakVisibility[i].load(std::memory_order_acquire)!=blockVisibility)peaks[i].store(blockPeaks[i],std::memory_order_relaxed);
            else {float old=peaks[i].load(std::memory_order_relaxed);while(old<blockPeaks[i]&&!peaks[i].compare_exchange_weak(old,blockPeaks[i],std::memory_order_relaxed)){} }
            peakVisibility[i].store(blockVisibility,std::memory_order_release);
        }
    }
    float consumePeak(int index)noexcept{
        if(index<0||index>=10)return 0;
        const auto visible=visibility.load(std::memory_order_acquire);
        if(peakVisibility[index].load(std::memory_order_acquire)!=visible)return 0;
        return peaks[index].exchange(0,std::memory_order_relaxed);
    }
private:
    static constexpr float quarterPi=.7853981633974483f;
    static float mix(float a,float b,float t)noexcept{return a+(b-a)*t;}
    static float linear(float db)noexcept{return std::pow(10.0f,db*.05f);}
    static float finiteSignal(float x)noexcept{return std::isfinite(x)?x:0;}
    struct Ramps {
        float enabled=0;
        std::array<float,softwareBusCount*busSourceCount> sends{};
        std::array<float,busSourceCount> masterSends{},inputL{},inputR{};
        std::array<float,softwareBusCount> gain{},toMaster{},busL{},busR{};
        std::array<float,2> masterGain{};
    } target,previous;
    BusMasterSnapshot state;
    bool initialised=false;
    std::array<float,10> blockPeaks{};
    std::array<std::atomic<float>,10> peaks{};
    std::array<std::atomic<std::uint64_t>,10> peakVisibility{};
    std::atomic<std::uint64_t> visibility{1};std::uint64_t blockVisibility=0;
};
}
