#pragma once
// Offline linear-phase windowed-sinc FIR design and deterministic single-sample convolution.
// Never apply a calculated FIR to live speaker outputs without validation and gain staging.
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <limits>
#include <stdexcept>
#include <vector>

namespace lah::fir {
constexpr double pi=3.14159265358979323846264338327950288;
enum class Mode { Lowpass, Highpass };
struct Design {
    std::vector<double> taps;
    double rate=0, cutoff=0, groupDelayMs=0;
    Mode mode=Mode::Lowpass;
};
inline Design make(Mode mode,double cutoff,double fs,int length){
    if(!std::isfinite(fs)||!std::isfinite(cutoff)||fs<8000||fs>192000 ||
       cutoff<10 || cutoff>=0.48*fs || length<33 || length>4097 || length%2==0)
        throw std::invalid_argument("FIR: rate/cutoff/taps invalid");
    Design d;d.rate=fs;d.cutoff=cutoff;d.mode=mode;
    d.groupDelayMs=1000.0*(length-1)/(2.0*fs);
    d.taps.resize(static_cast<std::size_t>(length));
    const int m=(length-1)/2;
    const double norm=2.0*cutoff/fs;
    double total=0;
    for(int i=0;i<length;++i){
        const int k=i-m;
        const double ideal=k==0?norm:std::sin(pi*norm*k)/(pi*k);
        const double w=0.42-0.5*std::cos(2*pi*i/(length-1))+
                         0.08*std::cos(4*pi*i/(length-1)); // Blackman window
        d.taps[static_cast<std::size_t>(i)]=ideal*w;
        total+=ideal*w;
    }
    if(!std::isfinite(total)||std::abs(total)<1e-12)throw std::invalid_argument("Invalid normalization");
    if(mode==Mode::Lowpass) {
        for(double& v:d.taps)v/=total;
    } else {
        // Invert the UNNORMALIZED windowed-sinc prototype, then normalize
        // the high-pass response at Nyquist, as in scipy.signal.firwin.
        for(double& v:d.taps)v=-v;
        d.taps[static_cast<std::size_t>(m)]+=1.0;
        // Match standard FIR high-pass passband normalization at Nyquist.
        // Scaling must be global to preserve exact coefficient symmetry.
        double alternating=0;
        for(std::size_t k=0;k<d.taps.size();++k)
            alternating+=d.taps[k]*(k%2==0?1.0:-1.0);
        if(std::abs(alternating)<1e-12)throw std::invalid_argument("FIR HP normalization failed");
        for(double& v:d.taps)v/=std::abs(alternating);
    }
    return d;
}
inline std::complex<double> response(const Design& d,double hz){
    if(d.taps.empty()||!std::isfinite(d.rate)||d.rate<=0||!std::isfinite(hz)||hz<0||hz>d.rate/2)return {};
    std::complex<double> h=0;
    const double w=-2*pi*hz/d.rate;
    for(std::size_t k=0;k<d.taps.size();++k)
        h+=d.taps[k]*std::polar(1.0,w*k);
    return h;
}
inline double magnitudeDb(const Design& d,double hz){
    return 20*std::log10(std::max(std::abs(response(d,hz)),1e-12));
}
// Reference direct-form convolution for impulse and block verification (offline ONLY).
class Convolver {
public:
    explicit Convolver(const Design& d): coefficient(d.taps),ring(coefficient.size()) {
        if(coefficient.empty())throw std::invalid_argument("FIR needs taps");
    }
    double tick(double x) noexcept {
        if(!std::isfinite(x))x=0;
        ring[pos]=x;
        double y=0;std::size_t k=pos;
        for(auto c:coefficient){y+=c*ring[k];if(k==0)k=ring.size()-1;else --k;}
        pos=(pos+1)%ring.size();
        return std::isfinite(y)?y:0;
    }
    void reset() noexcept {std::fill(ring.begin(),ring.end(),0);pos=0;}
private:
    std::vector<double> coefficient,ring;
    std::size_t pos=0;
};
} // namespace lah::fir

namespace lah::fir {
// Small-tap stereo convolution intended for experimental insert audition ONLY.
// Construct/replace only while AudioIODeviceCallback is detached. process() is
// allocation-free, lock-free, noexcept. Long FIR (>=513 taps), reconfiguration,
// per-channel IRs and partitioned FFT convolution are NOT provided here.
class StereoRealtimeFIR {
public:
    bool prepare(const Design& d, double deviceRate) {
        if (d.taps.size() < 33 || d.taps.size() > 257 || d.taps.size()%2==0 ||
            !std::isfinite(d.rate) || !std::isfinite(deviceRate) ||
            deviceRate<8000 || deviceRate>192000 || std::abs(d.rate-deviceRate)>0.1) {
            release();return false;
        }
        for(double tap:d.taps) if (!std::isfinite(tap)) {release();return false;}
        taps.assign(d.taps.begin(),d.taps.end());
        for(auto& channel : history) channel.assign(taps.size(),0.0f);
        positions={0,0};
        groupDelaySamples=static_cast<int>((taps.size()-1)/2);
        return true;
    }
    void clear() noexcept {for(auto& channel:history)std::fill(channel.begin(),channel.end(),0.0f);positions={0,0};}
    void release() {taps.clear();for(auto& channel:history)channel.clear();positions={0,0};groupDelaySamples=0;}
    bool ready() const noexcept {return !taps.empty();}
    int latencySamples() const noexcept {return ready()?groupDelaySamples:0;}
    void process(float* channelLeft,float* channelRight,int n) noexcept {
        if(!ready() || n<=0) return;
        for(int ch=0;ch<2;++ch) {
            float* destination=ch==0?channelLeft:channelRight;
            if(!destination)continue;
            auto& ring=history[static_cast<std::size_t>(ch)];
            auto& pos=positions[static_cast<std::size_t>(ch)];
            for(int sample=0;sample<n;++sample) {
                const double raw=std::isfinite(destination[sample])?destination[sample]:0.0;
                ring[pos]=static_cast<float>(raw);
                double output=0;auto idx=pos;
                for(double coeff:taps) {
                    output+=coeff*ring[idx];
                    idx=idx==0?ring.size()-1:idx-1;
                }
                destination[sample]=std::isfinite(output) &&
                    std::abs(output)<=std::numeric_limits<float>::max()
                    ?static_cast<float>(output):0.0f;
                pos=(pos+1)%ring.size();
            }
        }
    }
private:
    std::vector<double> taps;
    std::array<std::vector<float>,2> history{};
    std::array<std::size_t,2> positions{0,0};
    int groupDelaySamples=0;
};
} // namespace lah::fir
