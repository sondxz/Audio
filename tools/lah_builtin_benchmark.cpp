#include "../src/dsp/MixDspCore.h"
#include <chrono>
#include <iostream>
#include <memory>

int main(){
    auto rack=std::make_unique<lah::builtin::Rack>();auto native=std::make_unique<lah::builtin::Controls>();
    auto controls=std::make_unique<lah::dsp::Controls>();lah::dsp::Mixer mixer(*controls,rack.get(),native.get());
    mixer.prepare(48000);
    std::array<std::array<float,128>,32> input{},output{};
    std::array<const float*,32> ins{};std::array<float*,32> outs{};
    for(int ch=0;ch<32;++ch){ins[static_cast<std::size_t>(ch)]=input[static_cast<std::size_t>(ch)].data();outs[static_cast<std::size_t>(ch)]=output[static_cast<std::size_t>(ch)].data();
        for(int frame=0;frame<128;++frame)input[static_cast<std::size_t>(ch)][static_cast<std::size_t>(frame)]=.05f*std::sin(static_cast<float>((frame+ch)*.073));}
    std::cout<<"OFFLINE native-rack CPU observations, 48kHz /128 frames; period2666.67us. No driver, hardware or VST3.\n";
    for(int scenario=0;scenario<6;++scenario){
        for(auto& channel:native->slots)for(auto& slot:channel)lah::builtin::configureSlot(slot,lah::builtin::None,false);
        if(scenario==1)for(int ch=0;ch<8;++ch){lah::builtin::configureSlot((*native)[ch][0],lah::builtin::VocalClean,true);lah::builtin::configureSlot((*native)[ch][1],lah::builtin::Dynamics,true);}
        if(scenario==2)for(auto& channel:native->slots)for(auto& slot:channel)lah::builtin::configureSlot(slot,lah::builtin::Reverb96,true);
        if(scenario==3||scenario==4)for(auto& channel:native->slots)lah::builtin::configureSlot(channel[0],scenario==3?lah::builtin::AutoFeedback:lah::builtin::FeedbackGuard,true);
        if(scenario==5)for(auto& channel:native->slots)for(auto& slot:channel)lah::builtin::configureSlot(slot,lah::builtin::FeedbackGuard,true);
        rack->reset();std::vector<double> elapsed;elapsed.reserve(300);
        for(int block=0;block<350;++block){
            if(block==1&&scenario>=3)for(auto& channel:native->slots)for(auto& slot:channel)if(slot.enabled.load())slot.feedbackArmed.store(true);
            const auto start=std::chrono::steady_clock::now();mixer.process(ins.data(),32,outs.data(),32,128,true);
            const double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();
            if(block>=50)elapsed.push_back(us);
        }
        std::sort(elapsed.begin(),elapsed.end());
        const char* names[]={"32ch rack bypass","8ch x2 VocalClean+Dynamics","STRESS32ch x4 Reverb96","32ch x1 AUTO FB PRO armed","32ch x1 AI-FB LAB armed","STRESS32ch x4 AI-FB LAB armed"};
        std::cout<<names[scenario]
            <<": p50="<<elapsed[150]<<"us p99="<<elapsed[296]<<"us max="<<elapsed.back()<<"us\n";
    }
}
