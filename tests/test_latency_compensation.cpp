#include "../src/audio/LatencyCompensator.h"
#include "../src/fir/FIRCore.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

#define REQUIRE(x) do{if(!(x)){std::cerr<<"FAILED "<<__LINE__<<": "<<#x<<'\n';std::exit(1);}}while(false)

void checkLinearReference(int delay) {
    constexpr int channels=32;
    const int total=std::max(5000,delay+2048);
    lah::audio::LatencyCompensator alignment;
    REQUIRE(alignment.prepare(std::max(2048,delay)));REQUIRE(alignment.configure(delay));
    std::array<std::vector<float>,channels> sources,actual;
    std::array<float*,channels> outputs{};
    for(int ch=0;ch<channels;++ch) {
        sources[ch].resize(total);actual[ch].resize(total);
        for(int i=0;i<total;++i)
            sources[ch][i]=static_cast<float>((i*17+ch*29)%997-498)/1000.0f;
        actual[ch]=sources[ch];
    }
    const int blocks[]={1,3,64,127,513,1024};
    for(int offset=0,block=0;offset<total;++block) {
        const auto n=std::min(blocks[block%6],total-offset);
        for(int ch=0;ch<channels;++ch)outputs[ch]=actual[ch].data()+offset;
        alignment.process(outputs.data(),channels,n);offset+=n;
    }
    for(int ch=0;ch<channels;++ch)for(int i=0;i<total;++i) {
        const float expected=ch<2?sources[ch][i]:(i>=delay?sources[ch][i-delay]:0.0f);
        REQUIRE(actual[ch][i]==expected);
    }
}

int main() {
    for(int delay:{0,1,64,128,2048,65536})checkLinearReference(delay);
    lah::audio::LatencyCompensator alignment;
    REQUIRE(!alignment.configure(1));
    REQUIRE(!alignment.prepare(-1));
    REQUIRE(!alignment.prepare(lah::audio::LatencyCompensator::maximumSupportedDelay+1));
    REQUIRE(alignment.prepare(32));REQUIRE(alignment.maximumDelaySamples()==32);
    REQUIRE(!alignment.configure(-1) && alignment.latencySamples()==0);
    REQUIRE(!alignment.configure(33) && alignment.latencySamples()==0);
    REQUIRE(alignment.configure(32));
    std::array<float,128> left{},right{},dry{},extra{};
    left[0]=0.25f;right[0]=-0.25f;dry[0]=0.5f;extra[0]=0.75f;
    float* outputs[]={left.data(),right.data(),dry.data(),nullptr,extra.data()};
    alignment.process(outputs,5,128);
    REQUIRE(left[0]==0.25f && right[0]==-0.25f);
    REQUIRE(dry[0]==0 && dry[32]==0.5f && extra[32]==0.75f);
    left.fill(0.125f);right.fill(-0.125f);
    alignment.process(outputs,2,128);
    REQUIRE(left[127]==0.125f && right[127]==-0.125f);

    // A pending impulse must never return after mute/reset, even though the
    // large history allocation itself has not been cleared.
    REQUIRE(alignment.configure(32));dry.fill(0);dry[0]=1;
    alignment.process(outputs,3,8);alignment.invalidate();
    dry.fill(0);alignment.process(outputs,3,128);
    REQUIRE(std::all_of(dry.begin(),dry.end(),[](float x){return x==0;}));
    // A channel removed/null for a block cannot expose its old history later.
    dry.fill(0);dry[0]=1;alignment.process(outputs,3,8);
    outputs[2]=nullptr;alignment.process(outputs,3,128);
    outputs[2]=dry.data();dry.fill(0);alignment.process(outputs,3,128);
    REQUIRE(std::all_of(dry.begin(),dry.end(),[](float x){return x==0;}));
    dry[0]=std::numeric_limits<float>::infinity();alignment.invalidate();
    alignment.process(outputs,3,128);
    REQUIRE(std::all_of(dry.begin(),dry.end(),[](float x){return x==0;}));
    alignment.process(nullptr,32,128);alignment.process(outputs,2,0);
    REQUIRE(alignment.configure(0));dry.fill(0.125f);
    alignment.process(outputs,3,128);REQUIRE(dry[0]==0.125f && dry[127]==0.125f);

    // A known synthetic plugin delay plus the FIR's actual impulse peak align
    // with dry outputs; intentional speaker delay remains additive on both.
    constexpr int intentionalDelay=17,reportedPluginDelay=13;
    const auto design=lah::fir::make(lah::fir::Mode::Lowpass,3000,48000,129);
    lah::fir::StereoRealtimeFIR fir;REQUIRE(fir.prepare(design,48000));
    const int insertLatency=reportedPluginDelay+fir.latencySamples();
    REQUIRE(alignment.prepare(256));REQUIRE(alignment.configure(insertLatency));
    left.fill(0);right.fill(0);dry.fill(0);
    left[intentionalDelay+reportedPluginDelay]=right[intentionalDelay+reportedPluginDelay]=0.5f;
    dry[intentionalDelay]=0.5f;
    fir.process(left.data(),right.data(),128);alignment.process(outputs,3,128);
    const auto peak=std::max_element(left.begin(),left.end(),
        [](float a,float b){return std::abs(a)<std::abs(b);});
    REQUIRE(peak-left.begin()==intentionalDelay+insertLatency);
    REQUIRE(dry[intentionalDelay+insertLatency]==0.5f);
    std::cout<<"PASS: static reported-latency alignment, 32 channels, variable blocks, FIR peak, intentional delay, bounds/nulls/mute\n";
}
