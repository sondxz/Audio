#include "../src/spl/CalibratedSplCore.h"
#include "../src/fir/FIRCore.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <random>
#include <vector>
#define REQUIRE(x) do{if(!(x)){std::cerr<<"FAILED "<<__LINE__<<": "<<#x<<"\n";std::exit(1);}}while(false)
constexpr double pi=3.14159265358979323846264338327950288;
std::vector<float> tone(double frequency,double fs,double amplitude=0.1,int n=32768){
    std::vector<float> a(static_cast<std::size_t>(n));
    for(int i=0;i<n;++i)a[static_cast<std::size_t>(i)]=static_cast<float>(amplitude*std::sin(2*pi*frequency*i/fs));
    return a;
}
int main(){
    using namespace lah;
    auto calTone=tone(1000,48000);
    auto cal=spl::calibrate1000Hz(calTone,48000,94);
    REQUIRE(cal.valid); REQUIRE(std::abs(cal.offset-(94-20*std::log10(0.1/std::sqrt(2))))<0.001);
    REQUIRE(!spl::calibrate1000Hz(calTone,48000,92).valid);
    REQUIRE(!spl::calibrate1000Hz(tone(500,48000),48000,94).valid);
    auto silence=tone(1000,48000,0); REQUIRE(!spl::calibrate1000Hz(silence,48000,94).valid);
    auto clipping=tone(1000,48000,1.0); REQUIRE(!spl::calibrate1000Hz(clipping,48000,94).valid);
    auto z=spl::analyze(calTone,48000,cal);
    REQUIRE(z.valid);REQUIRE(std::abs(z.dbZ-94)<0.06);
    REQUIRE(std::abs(z.dbA-z.dbZ)<0.15);REQUIRE(std::abs(z.dbC-z.dbZ)<0.10);
    auto h=tone(125,48000);
    auto r125=spl::analyze(h,48000,cal);
    REQUIRE(r125.valid); REQUIRE(std::abs((r125.dbA-r125.dbZ)-(-16.2))<0.5);
    auto r8k=spl::analyze(tone(8000,48000),48000,cal);
    REQUIRE(r8k.valid);REQUIRE(std::abs((r8k.dbA-r8k.dbZ)-(-1.1))<0.5);
    REQUIRE(std::abs((r8k.dbC-r8k.dbZ)-(-3.0))<0.5);
    REQUIRE(!spl::analyze(calTone,44100,cal).valid);
    REQUIRE(!spl::calibrate1000Hz(calTone,std::numeric_limits<double>::quiet_NaN(),94).valid);
    REQUIRE(!spl::analyze(calTone,std::numeric_limits<double>::quiet_NaN(),cal).valid);
    auto corruptCalibration=cal;
    corruptCalibration.rate=std::numeric_limits<double>::quiet_NaN();
    REQUIRE(!spl::analyze(calTone,48000,corruptCalibration).valid);
    corruptCalibration=cal;corruptCalibration.offset=std::numeric_limits<double>::infinity();
    REQUIRE(!spl::analyze(calTone,48000,corruptCalibration).valid);
    auto invalid=calTone;invalid[100]=std::numeric_limits<float>::quiet_NaN();
    REQUIRE(!spl::analyze(invalid,48000,cal).valid);
    auto noise=calTone;
    std::mt19937 gen(42);std::uniform_real_distribution<float> dist(-0.25f,0.25f);
    for(auto& a:noise)a=dist(gen);
    REQUIRE(!spl::calibrate1000Hz(noise,48000,94).valid);
    auto interrupted=calTone;
    std::fill(interrupted.begin()+8192,interrupted.end(),0.0f);
    REQUIRE(!spl::calibrate1000Hz(interrupted,48000,94).valid);
    std::copy(noise.begin()+8192,noise.end(),interrupted.begin()+8192);
    REQUIRE(!spl::calibrate1000Hz(interrupted,48000,94).valid);
    auto unstable=calTone;
    for(std::size_t i=8192;i<unstable.size();++i)unstable[i]*=2.0f;
    REQUIRE(!spl::calibrate1000Hz(unstable,48000,94).valid);

    for(double fs:{44100.0,48000.0,96000.0}){
        for(int taps:{129,513,1025,2049}){
            for(auto mode:{fir::Mode::Lowpass,fir::Mode::Highpass}){
                auto d=fir::make(mode,3000,fs,taps);
                REQUIRE(d.taps.size()==static_cast<std::size_t>(taps));
                REQUIRE(std::abs(d.groupDelayMs-1000.0*(taps-1)/(2*fs))<1e-9);
                for(int k=0;k<taps;++k)REQUIRE(std::abs(d.taps[static_cast<std::size_t>(k)]-
                          d.taps[static_cast<std::size_t>(taps-1-k)])<1e-11);
                const auto passHz=mode==fir::Mode::Lowpass?0.0:fs/2.0;
                const auto stopHz=mode==fir::Mode::Lowpass?fs/2.0:0.0;
                REQUIRE(std::abs(fir::magnitudeDb(d,passHz))<0.15);
                REQUIRE(fir::magnitudeDb(d,stopHz)<-60);
                fir::Convolver c(d);
                for(int i=0;i<taps+20;++i){
                    const auto x=i==0?1.0:0.0;
                    const auto y=c.tick(x);
                    if(i<taps)REQUIRE(std::abs(y-d.taps[static_cast<std::size_t>(i)])<1e-12);
                    else REQUIRE(std::abs(y)<1e-12);
                }
            }
        }
    }
    try{(void)fir::make(fir::Mode::Lowpass,1000,48000,128);REQUIRE(false);}catch(const std::invalid_argument&){}
    std::cout<<"PASS: 1k acoustic reference gate; calibrated Z/A/C; weighting 125Hz/8kHz; "
             <<"clip/NaN/silence/noise; sample rate invalidation; FIR symmetry/anchor/stopband/impulse @3 rates, 4 tap counts\n";
}
