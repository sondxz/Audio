#pragma once
// Fixed-delay alignment for the single stereo OUT1/2 insert. Delays OUT3..32
// by its static reported VST3 latency + symmetric FIR group delay. Does not
// infer physical latency, compensate IIR phase, or validate a plugin's report.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace lah::audio {
class LatencyCompensator final {
public:
    static constexpr int channelLimit=32;
    static constexpr int maximumSupportedDelay=65536;

    // Allocation/configuration belongs to a muted, detached control path.
    // Maximum storage at the default bound is about 7.5 MiB of float history.
    bool prepare(int maxDelaySamples=maximumSupportedDelay) {
        if(maxDelaySamples<0 || maxDelaySamples>maximumSupportedDelay) {
            prepared=false;maximumDelay=0;latency=0;invalidate();return false;
        }
        const auto nextStride=static_cast<std::size_t>(maxDelaySamples)+1;
        history.assign(nextStride*compensatedChannels,0.0f);
        stride=nextStride;maximumDelay=maxDelaySamples;latency=0;
        positions.fill(0);validFrames.fill(0);channelEpoch.fill(0);epoch=1;
        prepared=true;
        return true;
    }

    // A failed configuration leaves alignment bypassed. The host must keep
    // output muted until it has resolved the unsupported insert latency.
    bool configure(int stereoLatencySamples) noexcept {
        invalidate();
        if(!prepared || stereoLatencySamples<0 || stereoLatencySamples>maximumDelay) {
            latency=0;return false;
        }
        latency=stereoLatencySamples;return true;
    }

    // No clearing of delay-sized buffers in the audio callback. A new epoch
    // starts each channel with zero valid history on its next process call.
    void invalidate() noexcept {++epoch;if(epoch==0)++epoch;}
    int latencySamples() const noexcept {return latency;}
    int maximumDelaySamples() const noexcept {return maximumDelay;}
    bool isPrepared() const noexcept {return prepared;}

    void process(float* const* outputs,int outputChannels,int frames) noexcept {
        if(!prepared || latency==0 || frames<=0)return;
        if(!outputs){invalidate();return;}
        const auto count=std::clamp(outputChannels,0,channelLimit);
        const auto lag=static_cast<std::size_t>(latency);
        for(int channel=2;channel<channelLimit;++channel) {
            const auto index=static_cast<std::size_t>(channel-2);
            auto* destination=channel<count?outputs[channel]:nullptr;
            if(!destination) {channelEpoch[index]=0;continue;}
            auto& valid=validFrames[index];
            auto& pos=positions[index];
            if(channelEpoch[index]!=epoch) {valid=0;channelEpoch[index]=epoch;}
            const auto base=index*stride;
            for(int sample=0;sample<frames;++sample) {
                const float input=std::isfinite(destination[sample])?destination[sample]:0.0f;
                history[base+pos]=input;
                const auto read=pos>=lag?pos-lag:pos+stride-lag;
                destination[sample]=valid>=lag?history[base+read]:0.0f;
                if(valid<lag)++valid;
                if(++pos==stride)pos=0;
            }
        }
    }
private:
    static constexpr std::size_t compensatedChannels=channelLimit-2;
    std::vector<float> history;
    std::array<std::size_t,compensatedChannels> positions{},validFrames{};
    std::array<std::uint64_t,compensatedChannels> channelEpoch{};
    std::size_t stride=0;
    std::uint64_t epoch=1;
    int maximumDelay=0,latency=0;
    bool prepared=false;
};
} // namespace lah::audio
