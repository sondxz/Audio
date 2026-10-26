#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <complex>

namespace lah::builtin {
constexpr int eqBandCount=8;
enum EqBandType {EqBell=0,EqLowShelf=1,EqHighShelf=2,EqHighPass=3,EqLowPass=4};
struct EqBandSnapshot {
    bool enabled=false;
    int type=EqBell;
    float frequency=1000,gainDb=0,q=1;
};
inline bool operator==(const EqBandSnapshot& a,const EqBandSnapshot& b)noexcept {
    return a.enabled==b.enabled && a.type==b.type && a.frequency==b.frequency && a.gainDb==b.gainDb && a.q==b.q;
}
inline bool operator!=(const EqBandSnapshot& a,const EqBandSnapshot& b)noexcept{return !(a==b);}
struct EqBandControl {
    std::atomic<bool> enabled{false};std::atomic<int> type{EqBell};
    std::atomic<float> frequency{1000},gainDb{0},q{1};
};
inline std::array<EqBandSnapshot,eqBandCount> defaultEqBands()noexcept {
    return {{{true,EqHighPass,20,0,.70710678f},{false,EqBell,100,0,.7f},
             {false,EqBell,250,0,1},{false,EqBell,1000,0,1},
             {false,EqBell,3000,0,1},{false,EqBell,8000,0,.7f},
             {false,EqHighShelf,12000,0,.70710678f},{false,EqLowPass,20000,0,.70710678f}}};
}
inline EqBandSnapshot boundedEqBand(EqBandSnapshot band)noexcept {
    if(band.type<EqBell || band.type>EqLowPass){band.type=EqBell;band.enabled=false;}
    band.frequency=std::isfinite(band.frequency)?std::clamp(band.frequency,20.0f,20000.0f):1000;
    band.gainDb=std::isfinite(band.gainDb)?std::clamp(band.gainDb,-24.0f,24.0f):0;
    band.q=std::isfinite(band.q)?std::clamp(band.q,.1f,18.0f):1;
    return band;
}
inline EqBandSnapshot snapshotEqBand(const EqBandControl& source)noexcept {
    return boundedEqBand({source.enabled.load(std::memory_order_acquire),source.type.load(std::memory_order_relaxed),
        source.frequency.load(std::memory_order_relaxed),source.gainDb.load(std::memory_order_relaxed),source.q.load(std::memory_order_relaxed)});
}
inline void recallEqBand(EqBandControl& target,EqBandSnapshot value)noexcept {
    value=boundedEqBand(value);target.enabled.store(false,std::memory_order_release);
    target.type.store(value.type,std::memory_order_relaxed);target.frequency.store(value.frequency,std::memory_order_relaxed);
    target.gainDb.store(value.gainDb,std::memory_order_relaxed);target.q.store(value.q,std::memory_order_relaxed);
    target.enabled.store(value.enabled,std::memory_order_release);
}
struct EqCoefficients {double b0=1,b1=0,b2=0,a1=0,a2=0;};
inline EqCoefficients eqCoefficients(EqBandSnapshot band,double sampleRate)noexcept {
    band=boundedEqBand(band);if(!band.enabled)return {};
    sampleRate=std::isfinite(sampleRate)?std::clamp(sampleRate,8000.0,192000.0):48000;
    const double w=6.2831853071795864769*std::min(static_cast<double>(band.frequency),sampleRate*.45)/sampleRate;
    const double c=std::cos(w),s=std::sin(w),alpha=s/(2*band.q),A=std::pow(10.,band.gainDb/40.);
    double b0=1,b1=0,b2=0,a0=1,a1=0,a2=0;
    switch(band.type){
        case EqHighPass:b0=(1+c)/2;b1=-(1+c);b2=b0;a0=1+alpha;a1=-2*c;a2=1-alpha;break;
        case EqLowPass:b0=(1-c)/2;b1=1-c;b2=b0;a0=1+alpha;a1=-2*c;a2=1-alpha;break;
        case EqLowShelf:{const double beta=2*std::sqrt(A)*alpha;
            b0=A*((A+1)-(A-1)*c+beta);b1=2*A*((A-1)-(A+1)*c);b2=A*((A+1)-(A-1)*c-beta);
            a0=(A+1)+(A-1)*c+beta;a1=-2*((A-1)+(A+1)*c);a2=(A+1)+(A-1)*c-beta;break;}
        case EqHighShelf:{const double beta=2*std::sqrt(A)*alpha;
            b0=A*((A+1)+(A-1)*c+beta);b1=-2*A*((A-1)+(A+1)*c);b2=A*((A+1)+(A-1)*c-beta);
            a0=(A+1)-(A-1)*c+beta;a1=2*((A-1)-(A+1)*c);a2=(A+1)-(A-1)*c-beta;break;}
        default:b0=1+alpha*A;b1=-2*c;b2=1-alpha*A;a0=1+alpha/A;a1=-2*c;a2=1-alpha/A;break;
    }
    return {b0/a0,b1/a0,b2/a0,a1/a0,a2/a0};
}
inline double eqBandResponseDb(const EqBandSnapshot& band,double frequency,double sampleRate)noexcept {
    sampleRate=std::isfinite(sampleRate)?std::clamp(sampleRate,8000.0,192000.0):48000;
    frequency=std::clamp(frequency,1.0,sampleRate*.499);
    const auto f=eqCoefficients(band,sampleRate);const auto z=std::polar(1.,-6.2831853071795864769*frequency/sampleRate),z2=z*z;
    return 20*std::log10(std::max(1e-12,std::abs((f.b0+f.b1*z+f.b2*z2)/(1.+f.a1*z+f.a2*z2))));
}
struct EqFilter {
    EqCoefficients coefficients{};double z1=0,z2=0;
    void reset()noexcept{z1=z2=0;}
    void configure(const EqBandSnapshot& band,double sampleRate)noexcept{coefficients=eqCoefficients(band,sampleRate);}
    float tick(float x)noexcept {
        const auto& f=coefficients;const double y=f.b0*x+z1;z1=f.b1*x-f.a1*y+z2;z2=f.b2*x-f.a2*y;
        if(!std::isfinite(y)||!std::isfinite(z1)||!std::isfinite(z2)){reset();return 0;}
        if(std::abs(z1)<1e-25)z1=0;if(std::abs(z2)<1e-25)z2=0;
        return std::isfinite(static_cast<float>(y))?static_cast<float>(y):0;
    }
};
}
