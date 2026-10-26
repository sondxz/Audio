#pragma once
// Importable angular / frequency-domain speaker transfer observations.
// No absolute SPL claim. Source must use a common time reference for phase.
#include "SubArrayCore.h"
#include <algorithm>
#include <cmath>
#include <complex>
#include <map>
#include <string>
#include <vector>

namespace lah::simulation {
struct PolarPoint {double frequencyHz=0, azimuthDeg=0, magnitudeDb=0, phaseDeg=0;};
struct PolarProfile {
    bool valid=false;
    std::string error;
    std::map<int,std::vector<PolarPoint>> byAngle;
    double minHz=0,maxHz=0;
    // Requires a complete, coherent set of azimuth slices with identical frequency sampling.
    static PolarProfile load(const std::vector<PolarPoint>& points) {
        PolarProfile p;
        if(points.empty() || points.size()>100000){p.error="Empty or excessive dataset";return p;}
        for(auto r:points){
            if(!std::isfinite(r.frequencyHz)||r.frequencyHz<10||r.frequencyHz>24000||
               !std::isfinite(r.azimuthDeg)||r.azimuthDeg<0||r.azimuthDeg>=360||
               !std::isfinite(r.magnitudeDb)||std::abs(r.magnitudeDb)>150||
               !std::isfinite(r.phaseDeg)||std::abs(r.phaseDeg)>1e6||
               std::abs(r.azimuthDeg-std::round(r.azimuthDeg))>1e-7) {
                p.error="Invalid frequency/angle/gain/phase";return p;
            }
            p.byAngle[static_cast<int>(r.azimuthDeg)].push_back(r);
        }
        if(p.byAngle.size()<4){p.error="At least 4 azimuth measurements required";return p;}
        std::size_t count=0;std::vector<double> hz;
        for(auto& [angle, rows]:p.byAngle){
            (void)angle;
            std::sort(rows.begin(),rows.end(),[](auto& a,auto& b){return a.frequencyHz<b.frequencyHz;});
            if(rows.size()<2){p.error="At least two frequency observations per angle";return p;}
            if(count && count!=rows.size()){p.error="Angles must share identical frequencies";return p;}
            if(count==0){count=rows.size();for(auto v:rows)hz.push_back(v.frequencyHz);}
            for(std::size_t i=0;i<rows.size();++i){
                if(std::abs(rows[i].frequencyHz-hz[i])>1e-6 ||
                   (i>0 && rows[i].frequencyHz<=rows[i-1].frequencyHz)){
                    p.error="Frequency grid mismatch or duplicates";return p;
                }
            }
        }
        p.minHz=hz.front();p.maxHz=hz.back();p.valid=true;return p;
    }
};
struct ComplexResponse {bool valid=false;std::complex<double> h{0,0};};
inline ComplexResponse profileResponse(const PolarProfile& profile,double hz,double angleDeg){
    if(!profile.valid||profile.byAngle.size()<4||!std::isfinite(hz)||hz<profile.minHz||hz>profile.maxHz||
       !std::isfinite(angleDeg))return {};
    angleDeg=std::fmod(angleDeg,360.0);
    if(angleDeg<0)angleDeg+=360;
    auto upper=profile.byAngle.lower_bound(static_cast<int>(std::ceil(angleDeg)));
    if(upper==profile.byAngle.end())upper=profile.byAngle.begin();
    auto lower=upper==profile.byAngle.begin()?std::prev(profile.byAngle.end()):std::prev(upper);
    // At a measured angle, use exactly the corresponding slice.
    if(auto exact=profile.byAngle.find(static_cast<int>(angleDeg));
       exact!=profile.byAngle.end() && exact->first==angleDeg) lower=upper=exact;
    auto interpolateFrequency=[hz](const std::vector<PolarPoint>& rows){
        auto hi=std::lower_bound(rows.begin(),rows.end(),hz,[](auto& row,double f){return row.frequencyHz<f;});
        if(hi==rows.end())return rows.back();
        if(hi==rows.begin())return rows.front();
        auto lo=std::prev(hi);
        const auto alpha=(std::log(hz)-std::log(lo->frequencyHz))/
             (std::log(hi->frequencyHz)-std::log(lo->frequencyHz));
        auto result=*lo;
        result.magnitudeDb+=(hi->magnitudeDb-lo->magnitudeDb)*alpha;
        // IMPORTANT: input phases need to be UNWRAPPED and share same time reference.
        result.phaseDeg+=(hi->phaseDeg-lo->phaseDeg)*alpha;
        return result;
    };
    const auto a=interpolateFrequency(lower->second);
    const auto b=interpolateFrequency(upper->second);
    auto minAngle=static_cast<double>(lower->first),maxAngle=static_cast<double>(upper->first);
    if(maxAngle<=minAngle)maxAngle+=360;
    if(angleDeg<minAngle)angleDeg+=360;
    const auto weight=(minAngle==maxAngle)?0.0:(angleDeg-minAngle)/(maxAngle-minAngle);
    const double gain=a.magnitudeDb+(b.magnitudeDb-a.magnitudeDb)*weight;
    const double phase=a.phaseDeg+(b.phaseDeg-a.phaseDeg)*weight;
    return {true,std::polar(std::pow(10.0,gain/20.0),phase*pi/180.0)};
}
// Relative pressure. Speaker profile applies same relative 1m transfer to every source.
inline ComplexResponse measuredPressure(const std::vector<Sub>& sources,Point3 listener,
                                        Parameters settings,const PolarProfile& profile){
    if(!profile.valid||!std::isfinite(settings.frequencyHz)||!std::isfinite(settings.speedOfSound)||
       settings.speedOfSound<=0 ||settings.frequencyHz<profile.minHz||settings.frequencyHz>profile.maxHz)return {};
    std::complex<double> value(0,0);
    for(const auto& sub:sources){
        if(!sub.enabled)continue;
        const auto dx=listener.x-sub.position.x,dy=listener.y-sub.position.y;
        const auto az=std::atan2(dx,dy)*180/pi;
        auto h=profileResponse(profile,settings.frequencyHz,az);
        if(!h.valid)return {};
        const double r=std::max(0.25,distance(sub.position,listener));
        const double gain=std::pow(10.0,std::clamp(sub.gainDb,-60.0,24.0)/20.0);
        const double phase=-2*pi*settings.frequencyHz*(r/settings.speedOfSound+sub.delayMs/1000.0);
        value+=h.h*(sub.invert?-1.0:1.0)*std::polar(gain/r,phase);
    }
    return {std::isfinite(value.real())&&std::isfinite(value.imag()),value};
}
inline Grid simulateMeasured2D(const std::vector<Sub>& sources,Parameters settings,int width,int height,
                               double spanX,double spanY,const PolarProfile& profile,double listenerHeight=1.2){
    Grid g;
    if(!profile.valid||!std::isfinite(settings.frequencyHz)||settings.frequencyHz<profile.minHz||settings.frequencyHz>profile.maxHz)return g;
    g.width=std::clamp(width,2,240);g.height=std::clamp(height,2,160);
    g.minDb=100;g.maxDb=-100;
    g.db.reserve(static_cast<std::size_t>(g.width*g.height));
    for(int iy=0;iy<g.height;++iy)for(int ix=0;ix<g.width;++ix){
        const double x=(static_cast<double>(ix)/(g.width-1)-0.5)*std::max(1.0,spanX);
        const double y=(static_cast<double>(iy)/(g.height-1)-0.25)*std::max(1.0,spanY);
        const auto measured=measuredPressure(sources,{x,y,listenerHeight},settings,profile);
        if(!measured.valid)return {};
        const auto level=static_cast<float>(std::clamp(20*std::log10(std::max(1e-8,std::abs(measured.h))),-100.0,60.0));
        g.db.push_back(level);g.minDb=std::min(g.minDb,level);g.maxDb=std::max(g.maxDb,level);
    }
    return g;
}
} // namespace lah::simulation
