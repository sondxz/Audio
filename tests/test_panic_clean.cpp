#include "../src/dsp/MixDspCore.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#define REQUIRE(x) do{if(!(x)){std::cerr<<"Failed line "<<__LINE__<<": "<<#x<<"\n";std::exit(1);}}while(false)
int main(){
    lah::dsp::Controls ctrl;
    lah::dsp::Mixer mixer(ctrl);mixer.prepare(48000);
    ctrl.fxSend[0].store(1);
    ctrl.delayReturn.store(1);
    ctrl.reverbReturn.store(1);
    ctrl.fxDelayMs.store(10);
    ctrl.fxFeedback.store(0.7f);
    float input[128]{},l[128]{},r[128]{};
    input[0]=0.4f;
    const float* ins[]={input};float* outs[]={l,r};
    for(int block=0;block<20;++block){
        mixer.process(ins,1,outs,2,128,true);
        input[0]=0;
    }
    // Actual prior FX energy exists. Muting must invalidate echoes in O(1),
    // not revive old delay/reverb audio when input is silent after resume.
    mixer.process(ins,1,outs,2,128,false);
    for(float sample:l)REQUIRE(sample==0);
    for(int block=0;block<20;++block){
        mixer.process(ins,1,outs,2,128,true);
        for(float sample:l)REQUIRE(std::abs(sample)<1e-6f);
        for(float sample:r)REQUIRE(std::abs(sample)<1e-6f);
    }
    std::cout<<"PASS: stale reverb/delay/tails purged by panic; safe re-enable after silence\n";
}
