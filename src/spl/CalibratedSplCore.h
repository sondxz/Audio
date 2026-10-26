#pragma once
// Research-grade spectral weighting / acoustic reference offset. Not IEC 61672 certified.
// Does NOT turn a consumer interface into a legal sound-level meter.
#include "../measurement/MeasurementCore.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace lah::spl {
struct Calibration {
    bool valid=false;
    double rate=0, calibratorDb=0, referenceDbfs=0, offset=0;
    std::string error;
};
struct Reading {
    bool valid=false;
    double dbZ=0, dbA=0, dbC=0, dbfs=0, sampleSeconds=0;
    std::string error;
};
inline double weightingDb(double f, char type) noexcept {
    if (type=='Z') return 0.0;
    if (!std::isfinite(f) || f<=0.0) return -300.0;
    const double f2=f*f;
    const double low = f2 + 20.6*20.6;
    const double high = f2 + 12200.0*12200.0;
    if (type=='C') {
        const double rc=(12200.0*12200.0*f2)/(low*high);
        return 20*std::log10(std::max(rc,1e-20))+0.06;
    }
    if (type=='A') {
        const double ra=(12200.0*12200.0*f2*f2)/
            (low*std::sqrt((f2+107.7*107.7)*(f2+737.9*737.9))*high);
        return 20*std::log10(std::max(ra,1e-20))+2.0;
    }
    return -300.0;
}
inline double rms(const std::vector<float>& samples) noexcept {
    if (samples.empty()) return 0;
    long double sum=0;
    for (float x:samples) {if(!std::isfinite(x))return 0;sum+=static_cast<long double>(x)*x;}
    return std::sqrt(static_cast<double>(sum/samples.size()));
}
// Must be same mic, interface, gain, sample rate and acoustic calibrator conditions.
// Spectral concentration rejects broadband noise and non-1 kHz calibration captures.
inline Calibration calibrate1000Hz(const std::vector<float>& captured,double fs,double referenceDb) {
    Calibration c;
    if (!std::isfinite(fs) || fs<8000 || fs>192000 || (referenceDb!=94.0 && referenceDb!=114.0)) {
        c.error="Chọn sample rate hợp lệ và calibrator 94 hoặc 114 dB @1kHz";return c;
    }
    if(captured.size()<static_cast<std::size_t>(measurement::fftSize)){
        c.error="Cần ít nhất 8192 mẫu calibration";return c;
    }
    const auto level=rms(captured);
    const auto peak=*std::max_element(captured.begin(),captured.end(),
                                      [](float a,float b){return std::abs(a)<std::abs(b);});
    if (!std::isfinite(peak) || std::abs(peak)>=0.98 || level<0.0001 || level>0.7){
        c.error="Tín hiệu quá nhỏ, vượt dải hiệu chuẩn hoặc gần clipping";return c;
    }
    std::array<measurement::C,measurement::fftSize> spectrum{};
    const auto n=measurement::fftSize;
    // The offset uses the WHOLE capture RMS, so validate its whole duration.
    // Checking only the first FFT could accept a tone followed by silence/noise.
    for(std::size_t start=0;;) {
        double energy=0,at1000=0,timeEnergy=0;
        for(int i=0;i<n;++i){
            const double w=0.5-0.5*std::cos(2*measurement::pi*i/(n-1));
            const double v=captured[start+static_cast<std::size_t>(i)];
            spectrum[static_cast<std::size_t>(i)]=v*w;
            timeEnergy+=v*v;
        }
        measurement::fft(spectrum);
        for(int k=1;k<n/2;++k){
            const double p=std::norm(spectrum[static_cast<std::size_t>(k)]);
            energy+=p;
            if(std::abs(k*fs/n-1000.0)<25.0)at1000+=p;
        }
        if(energy<=1e-16 || at1000/energy<0.85){
            c.error="Không phát hiện tone 1 kHz đủ thuần trên toàn bản thu (>85% năng lượng tại 1kHz)";return c;
        }
        // Operational stability gate only, not a claimed calibration tolerance.
        if(std::abs(20.0*std::log10(std::sqrt(timeEnergy/n)/level))>0.5){
            c.error="Mức tone hiệu chuẩn không ổn định trên toàn bản thu";return c;
        }
        const auto lastStart=captured.size()-static_cast<std::size_t>(n);
        if(start==lastStart)break;
        start=std::min(start+static_cast<std::size_t>(n),lastStart);
    }
    c.valid=true;c.rate=fs;c.calibratorDb=referenceDb;
    c.referenceDbfs=20.0*std::log10(level);
    c.offset=referenceDb-c.referenceDbfs;
    return c;
}
// FFT-based energy integration: a finite block spectral estimate, not Fast/Slow time weighting.
// Uses window-power-normalized one-sided spectrum and A/C response approximations.
inline Reading analyze(const std::vector<float>& samples,double fs,const Calibration& cal){
    Reading out;
    if (!cal.valid || !std::isfinite(fs) || !std::isfinite(cal.rate) ||
        !std::isfinite(cal.offset) || std::abs(fs-cal.rate)>0.1){out.error="Chưa hiệu chuẩn hoặc sample rate thay đổi";return out;}
    if(samples.size()<static_cast<std::size_t>(measurement::fftSize) ||
       fs<8000||fs>192000){out.error="Chưa có đủ audio";return out;}
    for(float v:samples)if(!std::isfinite(v) || std::abs(v)>=0.98f){
        out.error="Phát hiện giá trị không hợp lệ/clipping; không hiển thị SPL";return out;
    }
    const double z=rms(samples);
    if(z<1e-7){out.error="Mức tín hiệu quá nhỏ";return out;}
    std::array<measurement::C,measurement::fftSize> spectrum{};
    double powerA=0,powerC=0,powerZ=0,windowPower=0;
    int windows=0;
    constexpr int n=measurement::fftSize;
    for(std::size_t start=0;start+n<=samples.size();start+=n/2){
        double windowEnergy=0;
        for(int i=0;i<n;++i){
            const double w=0.5-0.5*std::cos(2*measurement::pi*i/(n-1));
            spectrum[static_cast<std::size_t>(i)]=samples[start+static_cast<std::size_t>(i)]*w;
            windowEnergy+=w*w;
        }
        measurement::fft(spectrum);
        const double norm=1.0/(n*windowEnergy);
        for(int k=0;k<=n/2;++k){
            const double hz=k*fs/n;
            const double factor=(k==0||k==n/2)?1.0:2.0;
            const double power=factor*std::norm(spectrum[static_cast<std::size_t>(k)])*norm;
            powerZ+=power;
            powerA+=power*std::pow(10.0,weightingDb(hz,'A')/10.0);
            powerC+=power*std::pow(10.0,weightingDb(hz,'C')/10.0);
        }
        ++windows;
        windowPower+=windowEnergy;
    }
    (void)windowPower;
    if(!windows){out.error="Không đủ cửa sổ FFT";return out;}
    powerZ/=windows;powerA/=windows;powerC/=windows;
    out.dbfs=20.0*std::log10(z);
    out.dbZ=10.0*std::log10(std::max(powerZ,1e-20))+cal.offset;
    out.dbA=10.0*std::log10(std::max(powerA,1e-20))+cal.offset;
    out.dbC=10.0*std::log10(std::max(powerC,1e-20))+cal.offset;
    out.sampleSeconds=samples.size()/fs;
    out.valid=std::isfinite(out.dbZ)&&std::isfinite(out.dbA)&&std::isfinite(out.dbC);
    return out;
}
} // namespace lah::spl
