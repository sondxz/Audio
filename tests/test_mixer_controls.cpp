#include "../src/dsp/MixDspCore.h"
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#define REQ(x) do{if(!(x)){std::cerr<<"FAILED "<<__LINE__<<": "<<#x<<"\n";std::exit(1);}}while(false)
int main(){
    lah::dsp::Controls c;
    lah::dsp::Mixer mixer(c);mixer.prepare(48000);
    c.route[0].store(1);c.route[lah::dsp::channels].store(1);
    float input[128]{},outL[128]{},outR[128]{};
    for(auto& v:input)v=0.2f;
    const float* i[]={input};float* o[]={outL,outR};
    mixer.process(i,1,o,2,128,true);
    REQ(std::abs(outL[127]-0.2f)<1e-4f && std::abs(outR[127]-0.2f)<1e-4f);
    c.inputPan[0].store(-1);
    mixer.process(i,1,o,2,128,true);
    REQ(std::abs(outL[127]-0.2f)<1e-4f && std::abs(outR[127])<1e-4f);
    c.inputPan[0].store(1);
    mixer.process(i,1,o,2,128,true);
    REQ(std::abs(outL[127])<1e-4f && std::abs(outR[127]-0.2f)<1e-4f);
    c.inputMute[0].store(1);
    mixer.process(i,1,o,2,128,true);
    REQ(std::abs(outL[127])<1e-4f && std::abs(outR[127])<1e-4f);
    c.inputMute[0].store(0);c.inputPan[0].store(0);
    mixer.process(i,1,o,2,128,true);
    REQ(std::abs(outL[127]-0.2f)<1e-4f && std::abs(outR[127]-0.2f)<1e-4f);
    std::cout<<"PASS: mixer input mute ramp, pan balance, identity fallback, stereo route\n";
}
