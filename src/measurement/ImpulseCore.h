#pragma once
// Experimental relative band-limited impulse/energy envelope derived from the
// H1 transfer-function bins. NOT a calibrated impulse-response measurement:
// missing/low-coherence bins are zeroed, which introduces spectral truncation.
#include "MeasurementCore.h"
#include <array>
#include <cmath>
#include <limits>

namespace lah::measurement {
struct RelativeImpulse {
    bool valid=false;
    double rate=0, peak=0, peakSeconds=0;
    int usedBins=0;
    std::array<double,fftSize> impulse{};
    std::array<double,fftSize> envelopeDb{};
};
inline RelativeImpulse deriveRelativeImpulse(const Result& tf) {
    RelativeImpulse out;
    if(!tf.valid || !tf.delayValid || !std::isfinite(tf.sampleRate) || tf.sampleRate<8000 || tf.sampleRate>384000)
        return out;
    std::array<C,fftSize> response{};
    for(int k=0;k<=fftSize/2;++k) {
        const auto idx=static_cast<std::size_t>(k);
        if(!tf.binReliable[idx] || !std::isfinite(tf.magnitudeDb[idx]) ||
           !std::isfinite(tf.phaseDeg[idx]))continue;
        const double amplitude=std::pow(10.0,std::clamp(tf.magnitudeDb[idx],-120.0,48.0)/20.0);
        const double radians=tf.phaseDeg[idx]*pi/180.0;
        response[idx]=std::polar(amplitude,radians);
        if(k>0 && k<fftSize/2) response[static_cast<std::size_t>(fftSize-k)]=std::conj(response[idx]);
        ++out.usedBins;
    }
    if(out.usedBins<30)return out;
    fft(response,true);
    double peak=0;int peakIndex=0;
    for(int i=0;i<fftSize;++i) {
        const double sample=response[static_cast<std::size_t>(i)].real();
        out.impulse[static_cast<std::size_t>(i)]=sample;
        if(std::abs(sample)>peak) {peak=std::abs(sample);peakIndex=i;}
    }
    if(!std::isfinite(peak) || peak<1e-10)return out;
    out.peak=peak;
    out.peakSeconds=peakIndex/tf.sampleRate;
    out.rate=tf.sampleRate;
    for(int i=0;i<fftSize;++i)
        out.envelopeDb[static_cast<std::size_t>(i)] =
            20.0*std::log10(std::max(std::abs(out.impulse[static_cast<std::size_t>(i)])/peak,1e-8));
    out.valid=true;
    return out;
}
} // namespace lah::measurement
