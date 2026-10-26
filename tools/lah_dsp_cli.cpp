// Deterministic offline mixer/DSP fixtures, not an audio-device measurement.
// stdout: actual input/output samples; stderr: normalized filter coefficients.
#include "../src/dsp/MixDspCore.h"
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

int main(int argc,char** argv) {
    if(argc!=4) {
        std::cerr<<"Usage: lah_dsp_cli hpf|peq|compressor|gate|routing|fx_delay|fx_reverb samplerate frames\n";
        return 2;
    }
    try {
        const std::string mode=argv[1];
        std::size_t consumed=0;
        const double rate=std::stod(argv[2],&consumed);
        if(consumed!=std::string(argv[2]).size() || !std::isfinite(rate) || rate<8000 || rate>192000)
            throw std::invalid_argument("Invalid sample rate");
        const int frames=std::stoi(argv[3],&consumed);
        if(consumed!=std::string(argv[3]).size() || frames<2048 || frames>131072)
            throw std::invalid_argument("Frames must be 2048..131072");
        lah::dsp::Controls controls;
        lah::dsp::Mixer mixer(controls);mixer.prepare(rate);
        std::array<std::vector<float>,2> input;
        std::array<std::vector<float>,32> output;
        for(auto& channel:input)channel.resize(static_cast<std::size_t>(frames));
        for(auto& channel:output)channel.resize(static_cast<std::size_t>(frames));
        input[0][0]=0.1f;
        if(mode=="hpf" || mode=="peq") {
            lah::dsp::Biquad coefficients;
            if(mode=="hpf") {
                controls.hpfHz[0].store(1000.0f);coefficients.highpass(1000,rate);
            } else {
                controls.eqHz[0].store(1500.0f);controls.eqQ[0].store(0.8f);controls.eqDb[0].store(9.0f);
                coefficients.peak(1500,static_cast<double>(0.8f),9,rate);
            }
            std::cerr<<std::setprecision(17)<<"{\"b\":["<<coefficients.b0<<','<<coefficients.b1<<','
                <<coefficients.b2<<"],\"a\":[1,"<<coefficients.a1<<','<<coefficients.a2<<"]}\n";
        } else if(mode=="compressor" || mode=="gate") {
            if(mode=="compressor") {
                controls.compThreshold[0].store(-20.0f);controls.compRatio[0].store(4.0f);
            } else controls.gateThreshold[0].store(-20.0f);
            controls.compAttackMs[0].store(10.0f);controls.compReleaseMs[0].store(100.0f);
            for(int i=0;i<frames;++i)
                input[0][static_cast<std::size_t>(i)]=mode=="compressor"
                    ?(i<frames/2?0.5f:0.05f):(i<frames/3?0.02f:i<2*frames/3?0.2f:0.02f);
        } else if(mode=="routing") {
            std::mt19937 rng(54321);std::uniform_real_distribution<float> distribution(-0.2f,0.2f);
            for(auto& channel:input)for(auto& sample:channel)sample=distribution(rng);
            for(auto& route:controls.route)route.store(0);
            controls.route[0].store(0.25f);controls.route[1].store(-0.5f);
            controls.route[32].store(-0.125f);controls.route[33].store(0.25f);
            controls.route[31*32].store(0.1f);controls.route[31*32+1].store(0.1f);
            controls.outputGain[0].store(-6.020599913f);controls.outputPolarity[0].store(-1);
            controls.outputDelayMs[1].store(static_cast<float>(1000.0*17/rate));
            controls.outputDelayMs[31].store(static_cast<float>(1000.0/rate));
        } else if(mode=="fx_delay" || mode=="fx_reverb") {
            for(auto& route:controls.route)route.store(0);
            controls.fxSend[0].store(1);
            controls.delayReturn.store(mode=="fx_delay"?0.5f:0.0f);
            controls.reverbReturn.store(mode=="fx_reverb"?1.0f:0.0f);
            controls.fxDelayMs.store(20.0f);controls.fxFeedback.store(0.5f);
        } else throw std::invalid_argument("Unknown DSP fixture");
        // Variable callback sizes expose discontinuities/state bugs at boundaries.
        constexpr int blocks[]={1,17,64,127,513};
        for(int offset=0,block=0;offset<frames;++block) {
            const int count=std::min(blocks[block%5],frames-offset);
            const float* sources[]={input[0].data()+offset,input[1].data()+offset};
            std::array<float*,32> sinks{};
            for(int channel=0;channel<32;++channel)sinks[channel]=output[channel].data()+offset;
            mixer.process(sources,2,sinks.data(),32,count,true);offset+=count;
        }
        std::cout<<"sample,input0,input1,output0,output1,output31\n"<<std::setprecision(10);
        for(int i=0;i<frames;++i) {
            const auto k=static_cast<std::size_t>(i);
            std::cout<<i<<','<<input[0][k]<<','<<input[1][k]<<','<<output[0][k]<<','
                <<output[1][k]<<','<<output[31][k]<<'\n';
        }
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 2;}
    return 0;
}
