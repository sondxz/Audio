#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <limits>
#include <string>
#include <vector>

namespace lah::simulation {
constexpr double pi=3.14159265358979323846;
struct Point3 { double x=0,y=0,z=0; };
struct Sub {
    Point3 position;
    double gainDb=0,delayMs=0;
    bool invert=false,enabled=true;
};
struct Parameters {double speedOfSound=343.0,frequencyHz=80.0;};
inline bool validPoint(Point3 point) noexcept {
    return std::isfinite(point.x)&&std::isfinite(point.y)&&std::isfinite(point.z)&&
        std::abs(point.x)<=100000&&std::abs(point.y)<=100000&&std::abs(point.z)<=100000;
}
inline bool validScene(const std::vector<Sub>& subs,Point3 listener,Parameters p) noexcept {
    if(!validPoint(listener)||!std::isfinite(p.frequencyHz)||p.frequencyHz<10||p.frequencyHz>300||
       !std::isfinite(p.speedOfSound)||p.speedOfSound<300||p.speedOfSound>370||subs.size()>128)return false;
    for(const auto& source:subs)if(source.enabled && (!validPoint(source.position)||
       !std::isfinite(source.gainDb)||source.gainDb<-60||source.gainDb>24||
       !std::isfinite(source.delayMs)||source.delayMs<0||source.delayMs>500))return false;
    return true;
}
inline double distance(Point3 a,Point3 b) noexcept {
    return std::sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z));
}
// Free-field omni point sources, direct wave ONLY. Relative pressure referenced to
// one unit-gain monopole at a distance of 1 metre. No room, ground or loudspeaker directivity.
inline std::complex<double> pressure(const std::vector<Sub>& subs,Point3 listener,Parameters p) noexcept {
    std::complex<double> result{};
    if(!validScene(subs,listener,p))return result;
    const auto frequency=p.frequencyHz;
    const auto c=p.speedOfSound;
    for(const auto& sub:subs) {
        if(!sub.enabled)continue;
        const auto r=std::max(0.25,distance(sub.position,listener));
        const auto gain=std::pow(10.0,std::clamp(sub.gainDb,-60.0,24.0)/20.0);
        const auto phase=-2*pi*frequency*(r/c+std::clamp(sub.delayMs,0.0,500.0)*0.001);
        result+=(sub.invert?-1.0:1.0)*std::polar(gain/r,phase);
    }
    return result;
}
inline double relativeDb(const std::vector<Sub>& subs,Point3 listener,Parameters p) noexcept {
    if(!validScene(subs,listener,p))return std::numeric_limits<double>::quiet_NaN();
    return std::clamp(20.0*std::log10(std::max(1e-8,std::abs(pressure(subs,listener,p)))),-100.0,60.0);
}

struct ArrivalEstimate {bool valid=false;double milliseconds=0;};
// Earliest geometric direct path, not a measured/group-delay estimator. A
// cancelled array can still have a geometric arrival but no meaningful phase.
inline ArrivalEstimate earliestArrival(const std::vector<Sub>& subs,Point3 point,double c) noexcept {
    if(!validScene(subs,point,{c,80}))return {};
    double earliest=std::numeric_limits<double>::infinity();
    for(const auto& source:subs)if(source.enabled)
        earliest=std::min(earliest,1000*std::max(0.25,distance(source.position,point))/c+source.delayMs);
    return {std::isfinite(earliest),std::isfinite(earliest)?earliest:0};
}
// Add only non-negative delays so all direct paths reach the selected point
// together. Polarity is preserved: an inverted source does not become coherent.
inline std::vector<Sub> alignAtPoint(const std::vector<Sub>& subs,Point3 point,double c) {
    if(!validScene(subs,point,{c,80}))return {};
    double latest=0;
    for(const auto& source:subs)if(source.enabled)
        latest=std::max(latest,1000*std::max(0.25,distance(source.position,point))/c+source.delayMs);
    auto aligned=subs;
    for(auto& source:aligned)if(source.enabled) {
        const auto arrival=1000*std::max(0.25,distance(source.position,point))/c+source.delayMs;
        source.delayMs+=std::max(0.0,latest-arrival);
        if(source.delayMs>500||source.delayMs<0)return {};
    }
    return aligned;
}
enum class ArrayPreset {Broadside=0,Cardioid=1,EndFire=2,Stereo=3,InvertedStack=4,Arc=5,ArcDelay=6};
inline const char* presetDescription(int id) noexcept {
    switch(static_cast<ArrayPreset>(id)) {
        case ArrayPreset::Broadside:return "Broadside: 4 sources across X, equal delay/polarity; audience +Y.";
        case ArrayPreset::Cardioid:return "Cardioid: rear at Y=-spacing, inverted and delayed spacing/c. Frequency-dependent gradient model.";
        case ArrayPreset::EndFire:return "End-fire: 4 sources along -Y; front delayed most (3,2,1,0)*spacing/c; audience +Y.";
        case ArrayPreset::Stereo:return "L/R: 2 equal sources at X=+/-spacing/2; interference depends on position/frequency.";
        case ArrayPreset::InvertedStack:return "Inverted stack: 2 front heights + 1 rear, rear inverted, +6.02dB and spacing/c delay; ideal monopoles.";
        case ArrayPreset::Arc:return "Physical arc: 4 sources, outer sources set back by arc depth; no electronic delays.";
        case ArrayPreset::ArcDelay:return "Arc-delay: straight row; non-negative delays align direct paths at (0, focus distance, 0.4m).";
    }
    return "Unknown array preset";
}
inline std::vector<Sub> preset(int id,double spacing,double c=343,double arcDepth=1.5,double focusDistance=12) {
    if(!std::isfinite(spacing)||!std::isfinite(c)||c<300||c>370||!std::isfinite(arcDepth)||
       arcDepth<0||arcDepth>10||!std::isfinite(focusDistance)||focusDistance<0.5||focusDistance>200)return {};
    spacing=std::clamp(spacing,0.25,5.0);
    // Audience direction is +Y; rear is -Y. Delay settings are examples only.
    if(id==0) // Broadside, four subs across stage
        return {{{-1.5*spacing,0,0.4},0,0,false,true},{{-0.5*spacing,0,0.4},0,0,false,true},
                {{0.5*spacing,0,0.4},0,0,false,true},{{1.5*spacing,0,0.4},0,0,false,true}};
    if(id==1) // Front/rear cardioid example: rear source time delayed and inverted
        return {{{0,0,0.4},0,0,false,true},{{0,-spacing,0.4},0,1000*spacing/c,true,true}};
    if(id==2) // End-fire +Y: front sub is delayed most, rear fires earliest.
        return {{{0,0,0.4},0,3000*spacing/c,false,true},{{0,-spacing,0.4},0,2000*spacing/c,false,true},
                {{0,-2*spacing,0.4},0,1000*spacing/c,false,true},{{0,-3*spacing,0.4},0,0,false,true}};
    if(id==3)return {{{-spacing/2,0,0.4},0,0,false,true},{{spacing/2,0,0.4},0,0,false,true}};
    if(id==4)return {{{0,0,0.4},0,0,false,true},{{0,0,0.4+spacing},0,0,false,true},
                     {{0,-spacing,0.4+spacing/2},20*std::log10(2.0),1000*spacing/c,true,true}};
    if(id==5 || id==6) {
        auto sources=preset(0,spacing,c);
        if(id==5)for(auto& source:sources) {
            const double normalized=source.position.x/(1.5*spacing);
            source.position.y=-arcDepth*normalized*normalized;
        }
        else sources=alignAtPoint(sources,{0,focusDistance,0.4},c);
        return sources;
    }
    return {};
}

struct FrequencyPoint {double hz=0,relativeDb=-100,phaseDeg=0;bool valid=false,phaseValid=false;};
struct FrequencyTrace {bool valid=false;double referenceDelayMs=0;std::vector<FrequencyPoint> points;};
inline FrequencyPoint responsePoint(std::complex<double> value,double hz,double referenceDelayMs=0) noexcept {
    FrequencyPoint result;result.hz=hz;
    if(!std::isfinite(value.real())||!std::isfinite(value.imag())||!std::isfinite(hz)||hz<10||hz>300||
       !std::isfinite(referenceDelayMs)||std::abs(referenceDelayMs)>1000000)return result;
    const auto amplitude=std::abs(value);
    if(!std::isfinite(amplitude))return result;
    result.relativeDb=std::clamp(20*std::log10(std::max(amplitude,1e-8)),-100.0,60.0);
    result.valid=true;result.phaseValid=amplitude>1e-8;
    if(result.phaseValid)result.phaseDeg=std::arg(value*std::polar(1.0,2*pi*hz*referenceDelayMs*0.001))*180/pi;
    return result;
}
inline FrequencyTrace frequencyResponse(const std::vector<Sub>& subs,Point3 point,double c,
                                       double lowHz=20,double highHz=300,int count=160,double referenceDelayMs=0) {
    FrequencyTrace trace;
    if(!validScene(subs,point,{c,80})||!std::isfinite(lowHz)||!std::isfinite(highHz)||
       lowHz<10||highHz>300||lowHz>=highHz||count<2||count>2048||!std::isfinite(referenceDelayMs)||
       std::abs(referenceDelayMs)>1000000)return trace;
    trace.referenceDelayMs=referenceDelayMs;trace.points.reserve(static_cast<std::size_t>(count));
    for(int i=0;i<count;++i) {
        const auto hz=lowHz*std::pow(highHz/lowHz,static_cast<double>(i)/(count-1));
        trace.points.push_back(responsePoint(pressure(subs,point,{c,hz}),hz,referenceDelayMs));
    }
    trace.valid=true;return trace;
}
struct Grid {int width=0,height=0;std::vector<float> db;float minDb=0,maxDb=0;};
inline Grid simulate2D(const std::vector<Sub>& subs,Parameters p,int width,int height,
                       double spanX,double spanY,double listenerHeight=1.2) {
    Grid g;
    if(!validScene(subs,{0,0,listenerHeight},p)||!std::isfinite(spanX)||!std::isfinite(spanY)||
       spanX<=0||spanY<=0||spanX>10000||spanY>10000)return g;
    g.width=std::clamp(width,2,240);g.height=std::clamp(height,2,160);
    g.db.resize(static_cast<std::size_t>(g.width*g.height));
    g.minDb=100;g.maxDb=-100;
    for(int iy=0;iy<g.height;++iy)for(int ix=0;ix<g.width;++ix) {
        const double x=(static_cast<double>(ix)/(g.width-1)-0.5)*std::max(1.0,spanX);
        const double y=(static_cast<double>(iy)/(g.height-1)-0.25)*std::max(1.0,spanY);
        const auto v=static_cast<float>(relativeDb(subs,{x,y,listenerHeight},p));
        g.db[static_cast<std::size_t>(iy*g.width+ix)]=v;
        g.minDb=std::min(v,g.minDb);g.maxDb=std::max(v,g.maxDb);
    }
    return g;
}
} // namespace lah::simulation
