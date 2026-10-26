#include "../src/fir/FIRCore.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>
#define REQ(x) do{if(!(x)){std::cerr<<"Failed line "<<__LINE__<<": "<<#x<<"\n";std::exit(1);}}while(false)
int main(){
    for(double rate:{44100.0,48000.0,96000.0}){
        for(int len:{129,257})for(auto mode:{lah::fir::Mode::Lowpass,lah::fir::Mode::Highpass}){
            auto d=lah::fir::make(mode,1000,rate,len);
            lah::fir::StereoRealtimeFIR host;
            REQ(host.prepare(d,rate));REQ(host.latencySamples()==(len-1)/2);
            REQ(!host.prepare(d,rate+1000));
            REQ(host.prepare(d,rate));
            constexpr int samples=4096;
            std::vector<float> left(samples),right(samples);
            left[0]=0.4f;right[0]=-0.2f;
            lah::fir::Convolver a(d),b(d);
            for(int offset=0;offset<samples;offset+=128){
                host.process(left.data()+offset,right.data()+offset,128);
            }
            for(int i=0;i<samples;++i){
                const auto expectA=a.tick(i==0?0.4:0.0);
                const auto expectB=b.tick(i==0?-0.2:0.0);
                REQ(std::abs(left[i]-expectA)<5e-8);
                REQ(std::abs(right[i]-expectB)<5e-8);
            }
            host.clear();
            std::fill(left.begin(),left.end(),0);std::fill(right.begin(),right.end(),0);
            host.process(left.data(),right.data(),samples);
            for(float value:left)REQ(value==0);
            host.release();REQ(!host.ready());
        }
    }
    // Reject 513+ taps on the live insert; still supported by OFFLINE design.
    auto longer=lah::fir::make(lah::fir::Mode::Lowpass,1200,48000,513);
    lah::fir::StereoRealtimeFIR fir;
    REQ(!fir.prepare(longer,48000));
    REQ(!fir.ready());
    auto malformed=lah::fir::make(lah::fir::Mode::Lowpass,1200,48000,129);
    malformed.rate=std::numeric_limits<double>::quiet_NaN();
    REQ(!fir.prepare(malformed,48000));
    malformed.rate=48000;
    REQ(!fir.prepare(malformed,std::numeric_limits<double>::infinity()));
    malformed.taps.resize(1);
    REQ(!fir.prepare(malformed,48000));
    malformed=lah::fir::make(lah::fir::Mode::Lowpass,1200,48000,129);
    malformed.taps[3]=std::numeric_limits<double>::quiet_NaN();
    REQ(!fir.prepare(malformed,48000));
    malformed.taps.assign(33,0);malformed.taps[0]=2.0;
    REQ(fir.prepare(malformed,48000));
    float overload=std::numeric_limits<float>::max();
    fir.process(&overload,nullptr,1);
    REQ(overload==0.0f); // finite double must not overflow during conversion to float
    fir.process(nullptr,nullptr,128);
    std::cout<<"PASS: short FIR streaming vs independent direct convolution; 3 rates, 2 modes, 2 tap sizes, stereo, mute/reset\n";
}
