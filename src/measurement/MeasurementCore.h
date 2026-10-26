#pragma once
// Independent offline mathematics: no GUI/driver dependency.
// Measurements are NOT certified SPL or IEC 61672 instruments.
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstddef>
#include <limits>
#include <vector>

namespace lah::measurement {
constexpr int fftSize=8192, captureCapacity=65536;
constexpr double pi=3.14159265358979323846264338327950288;
using C=std::complex<double>;

// Interleaved pair of atomic rings; producer publishes only after writing both channels.
// If producer advances too far while reader copies, snapshot fails rather than returning
// an intentionally partial buffer. Never allocate or lock inside put().
class Capture {
public:
    void put(const float* reference,const float* response,int n) noexcept {
        if(!response || n<=0)return;
        // Mark an in-progress write BEFORE touching ring entries. The committed
        // count alone cannot reveal a producer that is currently wrapping.
        writeEpoch.fetch_add(1,std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_release);
        const auto start=next.load(std::memory_order_relaxed);
        for(int i=0;i<n;++i) {
            const auto index=(start+static_cast<std::uint64_t>(i))%captureCapacity;
            ref[index].store(reference && std::isfinite(reference[i])?reference[i]:0.0f,std::memory_order_relaxed);
            mic[index].store(std::isfinite(response[i])?response[i]:0.0f,std::memory_order_relaxed);
        }
        next.store(start+static_cast<std::uint64_t>(n),std::memory_order_release);
        writeEpoch.fetch_add(1,std::memory_order_release);
    }
    bool snapshot(std::vector<float>& r,std::vector<float>& m,int count) const {
        count=std::clamp(count,0,captureCapacity/2);
        const auto before=writeEpoch.load(std::memory_order_acquire);
        if(before&1U)return false;
        const auto end=next.load(std::memory_order_acquire);
        if(end<static_cast<std::uint64_t>(count)||count==0)return false;
        r.resize(static_cast<std::size_t>(count));m.resize(static_cast<std::size_t>(count));
        for(int i=0;i<count;++i){
            const auto idx=(end-static_cast<std::uint64_t>(count)+static_cast<std::uint64_t>(i))%captureCapacity;
            r[static_cast<std::size_t>(i)]=ref[idx].load(std::memory_order_relaxed);
            m[static_cast<std::size_t>(i)]=mic[idx].load(std::memory_order_relaxed);
        }
        // This acquire fence pairs with the producer's release fence if any
        // relaxed sample load saw a newer write. Reject both partial pairs and
        // completed/reset/wrapping writes instead of publishing mixed data.
        std::atomic_thread_fence(std::memory_order_acquire);
        return before==writeEpoch.load(std::memory_order_relaxed) &&
            next.load(std::memory_order_acquire)-end < captureCapacity-static_cast<std::uint64_t>(count);
    }
    // Read a non-overlapping sequence for Leq. A discontinuity, overwrite or
    // device counter reset invalidates the history rather than inventing data.
    // Caller initializes cursor with writtenCount() when the session starts.
    bool readNext(std::vector<float>& r,std::vector<float>& m,int count,std::uint64_t& cursor) const {
        if(count<=0||count>captureCapacity/2)return false;
        const auto before=writeEpoch.load(std::memory_order_acquire);
        if(before&1U)return false;
        const auto end=next.load(std::memory_order_acquire);
        if(cursor>end || end-cursor>captureCapacity-static_cast<std::uint64_t>(count)) {
            cursor=end;return false;
        }
        if(end-cursor<static_cast<std::uint64_t>(count))return false;
        r.resize(static_cast<std::size_t>(count));m.resize(static_cast<std::size_t>(count));
        for(int i=0;i<count;++i) {
            const auto index=(cursor+static_cast<std::uint64_t>(i))%captureCapacity;
            r[static_cast<std::size_t>(i)]=ref[index].load(std::memory_order_relaxed);
            m[static_cast<std::size_t>(i)]=mic[index].load(std::memory_order_relaxed);
        }
        std::atomic_thread_fence(std::memory_order_acquire);
        if(before!=writeEpoch.load(std::memory_order_relaxed))return false;
        if(next.load(std::memory_order_acquire)-cursor>captureCapacity-static_cast<std::uint64_t>(count)){
            cursor=next.load(std::memory_order_acquire);return false;
        }
        cursor+=static_cast<std::uint64_t>(count);
        return true;
    }
    std::uint64_t writtenCount() const noexcept {return next.load(std::memory_order_acquire);}
    // Only the stopped/detached producer may reset; readers can remain active.
    void reset() noexcept {
        writeEpoch.fetch_add(1,std::memory_order_acq_rel);
        next.store(0,std::memory_order_release);
        writeEpoch.fetch_add(1,std::memory_order_release);
    }
private:
    std::array<std::atomic<float>,captureCapacity> ref{},mic{};
    std::atomic<std::uint64_t> next{0};
    std::atomic<std::uint64_t> writeEpoch{0};
};

// Radix-2 DFT, forward convention X[k] = sum x[n] exp(-j 2pi kn/N).
template<class Buffer> inline void fft(Buffer& a,bool inverse=false){
    const auto n=a.size();
    for(std::size_t i=1,j=0;i<n;++i){
        std::size_t bit=n>>1;
        for(;j&bit;bit>>=1)j^=bit;
        j^=bit;if(i<j)std::swap(a[i],a[j]);
    }
    for(std::size_t len=2;len<=n;len*=2){
        const C delta=std::polar(1.0,(inverse?2.0:-2.0)*pi/static_cast<double>(len));
        for(std::size_t i=0;i<n;i+=len){
            C w=1;
            for(std::size_t j=0;j<len/2;++j){
                const C u=a[i+j],v=a[i+j+len/2]*w;
                a[i+j]=u+v;a[i+j+len/2]=u-v;w*=delta;
            }
        }
    }
    if(inverse)for(auto& v:a)v/=static_cast<double>(n);
}

struct Result {
    bool valid=false,hasRta=false,delayValid=false;
    int delaySamples=0;
    double delayMs=0,delayConfidence=0,delayPeakCorrelation=0;
    double sampleRate=0,meanCoherence=0,referenceRmsDbfs=-160,measurementRmsDbfs=-160;
    int segments=0,validBins=0,reliableBins=0;
    std::array<double,fftSize/2+1> rtaDb{},magnitudeDb{},phaseDeg{},coherence{};
    std::array<bool,fftSize/2+1> binValid{},binReliable{};
};

inline double safeRms(const std::vector<float>& signal) noexcept {
    if(signal.empty())return 0;
    long double energy=0;
    for(float v:signal)if(std::isfinite(v))energy+=static_cast<long double>(v)*v;
    return std::sqrt(static_cast<double>(energy/signal.size()));
}
inline double toDb(double amplitude) noexcept {return 20.0*std::log10(std::max(amplitude,1e-8));}

struct DelayEstimate{bool valid=false;int samples=0;double peak=0,peakToSidelobe=0;};

// Full linear GCC-PHAT (zero-padded FFT) prevents false modulo-8192 delay wrapping.
// Peak is tested against sidelobes and input/output overall correlation. Does not claim
// correctness for periodic, low-SNR, clock-drifting or highly reverberant measurements.
inline DelayEstimate estimateDelay(const std::vector<float>& reference,
                                   const std::vector<float>& measured,
                                   double sampleRate,double maxDelayMs=250.0){
    DelayEstimate d;
    const auto n=reference.size();
    if(n!=measured.size()||n<fftSize*2||!std::isfinite(sampleRate)||
       !std::isfinite(maxDelayMs)||sampleRate<8000||sampleRate>384000||
       safeRms(reference)<1e-4||safeRms(measured)<1e-5)return d;
    std::size_t padded=1;
    while(padded<2*n-1)padded*=2;
    std::vector<C> x(padded),y(padded);
    // Remove DC bias prior to estimating transport delay.
    double mx=0,my=0;
    for(std::size_t i=0;i<n;++i){mx+=std::isfinite(reference[i])?reference[i]:0;
                                    my+=std::isfinite(measured[i])?measured[i]:0;}
    mx/=static_cast<double>(n);my/=static_cast<double>(n);
    for(std::size_t i=0;i<n;++i){
        x[i]=(std::isfinite(reference[i])?reference[i]:0)-mx;
        y[i]=(std::isfinite(measured[i])?measured[i]:0)-my;
    }
    fft(x);fft(y);
    // Reject narrowband / periodic references: one sharp cross-correlation peak
    // does NOT prove delay when the spectrum lacks broadband excitation.
    double arithmeticPower=0,logPower=0;
    for(std::size_t k=1;k<padded/2;++k){
        const double power=std::max(1e-25,std::norm(x[k]));
        arithmeticPower+=power;logPower+=std::log(power);
    }
    const double count=static_cast<double>(padded/2-1);
    const double spectralFlatness=std::exp(logPower/count)/std::max(arithmeticPower/count,1e-25);
    if(spectralFlatness<0.005)return d;
    for(std::size_t k=0;k<padded;++k){
        const C cross=y[k]*std::conj(x[k]);
        const double mag=std::abs(cross);
        x[k]=mag>1e-14?cross/mag:C(0,0);
    }
    fft(x,true);
    const int maxLag=std::max(0,std::min(static_cast<int>(n/2)-1,
        static_cast<int>(std::lround(sampleRate*std::clamp(maxDelayMs,1.0,2000.0)*0.001))));
    double best=0;int bestLag=0;
    for(int lag=-maxLag;lag<=maxLag;++lag){
        const auto idx=lag>=0?static_cast<std::size_t>(lag):padded-static_cast<std::size_t>(-lag);
        const double magnitude=std::abs(x[idx]);
        if(magnitude>best){best=magnitude;bestLag=lag;}
    }
    double secondary=0,meanSide=0;int sideCount=0;
    for(int lag=-maxLag;lag<=maxLag;++lag){
        if(std::abs(lag-bestLag)<=8)continue;
        const auto idx=lag>=0?static_cast<std::size_t>(lag):padded-static_cast<std::size_t>(-lag);
        const double magnitude=std::abs(x[idx]);
        secondary=std::max(secondary,magnitude);
        meanSide+=magnitude;++sideCount;
    }
    const double ratio=best/std::max(secondary,1e-12);
    const double prominence=best/std::max(sideCount?meanSide/sideCount:0.0,1e-12);
    d.peak=best;d.peakToSidelobe=ratio;
    // Relative criteria; application should still validate against an independent loopback.
    d.valid=best>=0.02 && ratio>=1.5 && prominence>=8;
    if(d.valid)d.samples=bestLag;
    return d;
}

// H1 = S_yx/S_xx; coherence = |S_yx|^2/(S_xx*S_yy), Hann window and
// 50% overlap. Delay compensation explicitly precedes TF FFTs. RTA dBFS/bin
// is window-power-corrected RMS spectral-bin level, NOT calibrated acoustic SPL.
inline Result analyze(const std::vector<float>& reference,const std::vector<float>& measured,double rate){
    Result result;result.sampleRate=rate;
    result.rtaDb.fill(-160.0);result.magnitudeDb.fill(-160.0);
    if(!std::isfinite(rate)||rate<8000||rate>384000||reference.size()!=measured.size()||reference.size()<fftSize*2)return result;
    result.referenceRmsDbfs=toDb(safeRms(reference));
    result.measurementRmsDbfs=toDb(safeRms(measured));
    result.hasRta=result.measurementRmsDbfs>-120.0;
    if(result.referenceRmsDbfs>-80.0 && result.measurementRmsDbfs>-100.0){
        const auto estimated=estimateDelay(reference,measured,rate);
        result.delayValid=estimated.valid;
        result.delaySamples=estimated.samples;
        result.delayMs=1000.0*estimated.samples/rate;
        result.delayConfidence=estimated.peakToSidelobe;
        result.delayPeakCorrelation=estimated.peak;
    }
    const int offset=std::abs(result.delaySamples);
    const std::size_t available=reference.size()-static_cast<std::size_t>(offset);
    const std::size_t refOffset=result.delaySamples<0?static_cast<std::size_t>(offset):0;
    const std::size_t micOffset=result.delaySamples>0?static_cast<std::size_t>(offset):0;
    // Analysis is worker/offline-only. Keep the large FFT workspace off the
    // default Windows thread stack; no allocation enters Capture::put().
    std::vector<double> xx(fftSize/2+1),yy(fftSize/2+1);
    std::vector<C> yx(fftSize/2+1),x(fftSize),y(fftSize);
    double windowEnergy=0;
    for(int i=0;i<fftSize;++i){
        const auto w=0.5-0.5*std::cos(2*pi*i/(fftSize-1));
        windowEnergy+=w*w;
    }
    for(std::size_t start=0;start+fftSize<=available;start+=fftSize/2){
        for(int i=0;i<fftSize;++i){
            const double w=0.5-0.5*std::cos(2*pi*i/(fftSize-1));
            const auto ri=refOffset+start+static_cast<std::size_t>(i);
            const auto mi=micOffset+start+static_cast<std::size_t>(i);
            x[static_cast<std::size_t>(i)]=(std::isfinite(reference[ri])?reference[ri]:0)*w;
            y[static_cast<std::size_t>(i)]=(std::isfinite(measured[mi])?measured[mi]:0)*w;
        }
        fft(x);fft(y);
        for(std::size_t k=0;k<xx.size();++k){
            xx[k]+=std::norm(x[k]);yy[k]+=std::norm(y[k]);yx[k]+=y[k]*std::conj(x[k]);
        }
        ++result.segments;
    }
    if(result.segments<3)return result;
    const double binsRmsNorm=1.0/(static_cast<double>(fftSize)*windowEnergy*result.segments);
    double sum=0;
    const bool refStrong=result.referenceRmsDbfs>-80.0;
    for(std::size_t k=0;k<xx.size();++k){
        const double factor=(k==0||k==fftSize/2)?1.0:2.0;
        const double rmsRef=std::sqrt(std::max(0.0,factor*xx[k]*binsRmsNorm));
        const double rmsY=std::sqrt(std::max(0.0,factor*yy[k]*binsRmsNorm));
        result.rtaDb[k]=toDb(rmsY);
        // Use a reference-floor threshold PER BIN to suppress unreliable frequencies.
        const bool present=refStrong && rmsRef>std::pow(10.0,-105.0/20.0) && yy[k]>1e-16;
        result.binValid[k]=present;
        if(!present){result.magnitudeDb[k]=-160;result.phaseDeg[k]=0;result.coherence[k]=0;continue;}
        const C h=yx[k]/xx[k];
        result.magnitudeDb[k]=toDb(std::abs(h));
        result.phaseDeg[k]=std::arg(h)*180.0/pi;
        result.coherence[k]=std::clamp(std::norm(yx[k])/(xx[k]*yy[k]),0.0,1.0);
        const double hz=k*rate/fftSize;
        if(hz>=20.0 && hz<=std::min(20000.0,rate/2.0)){
            ++result.validBins;
            result.binReliable[k]=result.coherence[k]>=0.6;
            if(result.binReliable[k])++result.reliableBins;
            sum+=result.coherence[k];
        }
    }
    result.meanCoherence=result.validBins?sum/result.validBins:0.0;
    result.valid=result.validBins>=20;
    return result;
}
} // namespace lah::measurement
