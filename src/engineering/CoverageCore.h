#pragma once
// Nominal -6dB geometry ONLY: 2D sound ray intersection, no loudspeaker polar
// measurement, no wave interference/SPL. Intended for front-fill first estimates.
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <locale>
#include <string>
#include <vector>
#include "EngineeringCore.h"

namespace lah::engineering {
struct FillSource {double x{}, y{}, headingDeg{};}; // +Y is audience
struct FillCoverage {
    std::vector<FillSource> sources;
    std::vector<double> sampledX;
    std::vector<int> coveringSources;
    double nominalWidthM{}, audienceDistanceM{}, beamWidthDeg{},
           fractionCovered{}, fractionOverlap{}, maxUncoveredGapM{},
           centerSpacingM{};
};
struct AudiencePoint { double x{}, y{}; };
struct CoverageAnalysis {
    std::vector<int> counts;
    double coveredFraction{}, overlapFraction{};
};
// Azimuth: 0 degrees faces +Y, positive turns toward +X. Beam width is
// the full nominal manufacturer angle; this is a geometry model, not SPL.
inline double wrappedAzimuth(double degrees) {
    check(degrees,-1e9,1e9,"Azimuth out of range");
    return std::remainder(degrees,360.0);
}
inline void validateFillSource(const FillSource& source) {
    check(source.x,-1000,1000,"Source X out of range");
    check(source.y,-1000,1000,"Source Y out of range");
    (void)wrappedAzimuth(source.headingDeg);
}
inline bool insideNominalBeam(const FillSource& source, AudiencePoint point, double fullAngleDeg) {
    validateFillSource(source);
    check(point.x,-2000,2000,"Audience X out of range");
    check(point.y,-2000,2000,"Audience Y out of range");
    check(fullAngleDeg,1,180,"Beam angle out of range");
    const double dx=point.x-source.x,dy=point.y-source.y;
    if(std::hypot(dx,dy)<1e-9)return false; // bearing at the source is undefined
    const double bearing=std::atan2(dx,dy)*180.0/pi;
    return std::abs(wrappedAzimuth(bearing-wrappedAzimuth(source.headingDeg)))<=fullAngleDeg/2+1e-10;
}
inline CoverageAnalysis analyseCoverage(const std::vector<FillSource>& sources,
                                        const std::vector<AudiencePoint>& points,double fullAngleDeg) {
    if(sources.empty() || sources.size()>80 || points.empty() || points.size()>100000)
        throw std::invalid_argument("Coverage needs 1..80 sources and 1..100000 audience points");
    check(fullAngleDeg,1,180,"Beam angle out of range");
    for(const auto& source:sources)validateFillSource(source);
    CoverageAnalysis result;
    result.counts.reserve(points.size());
    std::size_t covered=0,overlap=0;
    for(const auto& point:points){
        int count=0;
        for(const auto& source:sources)count+=insideNominalBeam(source,point,fullAngleDeg)?1:0;
        result.counts.push_back(count);covered+=count>0;overlap+=count>1;
    }
    result.coveredFraction=static_cast<double>(covered)/static_cast<double>(points.size());
    result.overlapFraction=static_cast<double>(overlap)/static_cast<double>(points.size());
    return result;
}
inline std::vector<FillSource> circularFill(int count,double radiusM,double spreadDeg=360,double rotationDeg=0) {
    if(count<1 || count>80)throw std::invalid_argument("Speaker count must be 1..80");
    check(radiusM,.1,200,"Array radius out of range");
    check(spreadDeg,1,360,"Arc spread out of range");
    const double rotation=wrappedAzimuth(rotationDeg);
    const bool ring=spreadDeg==360;
    std::vector<FillSource> result;result.reserve(static_cast<std::size_t>(count));
    for(int index=0;index<count;++index){
        const double azimuth=rotation+(ring?360.0*index/count:
            (count==1?0:-spreadDeg/2+spreadDeg*index/(count-1)));
        const double radians=azimuth*pi/180;
        // A fan arc is tangent to stage origin; a full ring is centred there.
        result.push_back({radiusM*std::sin(radians),radiusM*std::cos(radians)-(ring?0:radiusM),wrappedAzimuth(azimuth)});
    }
    return result;
}
inline std::vector<AudiencePoint> audienceLine(double widthM,double y,int samples=501) {
    check(widthM,1,200,"Audience width out of range");check(y,-1000,1000,"Audience Y out of range");
    if(samples<2 || samples>100000)throw std::invalid_argument("Audience sample count out of range");
    std::vector<AudiencePoint> points;points.reserve(static_cast<std::size_t>(samples));
    for(int index=0;index<samples;++index)points.push_back({-widthM/2+widthM*index/(samples-1),y});
    return points;
}
inline std::vector<AudiencePoint> audienceRing(double radiusM,int samples=720) {
    check(radiusM,.1,1000,"Audience radius out of range");
    if(samples<3 || samples>100000)throw std::invalid_argument("Audience sample count out of range");
    std::vector<AudiencePoint> points;points.reserve(static_cast<std::size_t>(samples));
    for(int index=0;index<samples;++index){const double angle=2*pi*index/samples;
        points.push_back({radiusM*std::sin(angle),radiusM*std::cos(angle)});}
    return points;
}
inline std::string fillSourcesCsv(const std::vector<FillSource>& sources) {
    if(sources.empty() || sources.size()>80)throw std::invalid_argument("Speaker count must be 1..80");
    std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(17)<<"x_m,y_m,heading_deg\n";
    for(const auto& source:sources){validateFillSource(source);out<<source.x<<','<<source.y<<','<<source.headingDeg<<'\n';}
    return out.str();
}
inline std::vector<FillSource> parseFillSourcesCsv(const std::string& csv) {
    if(csv.size()>65536)throw std::invalid_argument("Layout CSV is too large");
    std::istringstream input(csv);input.imbue(std::locale::classic());std::string line;
    std::vector<FillSource> sources;bool first=true;
    while(std::getline(input,line)){
        if(line.find_first_not_of(" \t\r")==std::string::npos)continue;
        if(first && (line=="x_m,y_m,heading_deg" || line=="x_m,y_m,heading_deg\r")){first=false;continue;}
        first=false;std::istringstream row(line);row.imbue(std::locale::classic());
        FillSource source;char comma1=0,comma2=0;
        if(!(row>>source.x>>comma1>>source.y>>comma2>>source.headingDeg) || comma1!=',' || comma2!=',')
            throw std::invalid_argument("Each CSV row needs x_m,y_m,heading_deg");
        row>>std::ws;if(!row.eof())throw std::invalid_argument("Unexpected extra CSV fields");
        validateFillSource(source);sources.push_back(source);
        if(sources.size()>80)throw std::invalid_argument("CSV supports at most 80 speakers");
    }
    if(sources.empty())throw std::invalid_argument("CSV contains no speakers");
    return sources;
}
inline double beamIntersectionAtAudience(double sourceX, double sourceY,
                                        double audienceY, double headingDeg) {
    check(sourceX,-1000,1000,"Source X out of range");check(sourceY,-1000,1000,"Source Y out of range");
    check(audienceY,-1000,1000,"Audience Y out of range");check(headingDeg,-89.1,89.1,"Beam points away from audience");
    if(audienceY<=sourceY)throw std::invalid_argument("Audience must be in front of beam plane");
    const auto rad=headingDeg*pi/180.0;
    if (std::abs(rad)>=pi*0.495) throw std::invalid_argument("Beam points away from audience");
    return sourceX+(audienceY-sourceY)*std::tan(rad);
}
inline FillCoverage calculateFill(double targetWidthM, double audienceDistanceM,
                                 double fullAngleDeg, double overlapFraction,
                                 int fixedCount=0, double curvatureM=0) {
    check(targetWidthM, 1, 200, "Coverage width out of range");
    check(audienceDistanceM, 1, 200, "Listener distance out of range");
    check(fullAngleDeg, 10, 140, "Angle out of range");
    check(overlapFraction, 0, 0.75, "Nominal overlap out of range");
    check(curvatureM, 0, 20, "Arc curvature out of range");
    if(fixedCount<0 || fixedCount>80) throw std::invalid_argument("Invalid speaker count");
    const double footprint=2*audienceDistanceM*std::tan(fullAngleDeg*pi/360.0);
    // Number for target extent plus desired overlap; true directivity unknown.
    const double maxPitch=footprint*(1-overlapFraction);
    const int count=fixedCount>0?fixedCount:std::clamp(
        static_cast<int>(std::ceil(std::max(0.0,targetWidthM-footprint)/maxPitch))+1,1,80);
    FillCoverage r;
    r.nominalWidthM=targetWidthM;r.audienceDistanceM=audienceDistanceM;r.beamWidthDeg=fullAngleDeg;
    r.sources.reserve(static_cast<std::size_t>(count));
    const double pitch=(count==1)?0:std::max(0.0,targetWidthM-footprint)/(count-1);
    r.centerSpacingM=pitch;
    const double start=-0.5*(count-1)*pitch;
    for (int i=0;i<count;++i) {
        const double x=start+i*pitch;
        const double normalized=(count==1)?0:(2.0*i/(count-1)-1.0);
        const double y=-curvatureM*normalized*normalized;
        // Arc aims speakers toward uniformly spaced target centers; straight
        // array (curvature=0) shoots forward, as in the screenshots.
        const double targetX=-0.5*targetWidthM+(i+0.5)*targetWidthM/count;
        const double heading=(curvatureM>0) ? std::atan2(targetX-x,audienceDistanceM-y)*180.0/pi : 0.0;
        r.sources.push_back({x,y,heading});
    }
    constexpr int samples=501;
    r.sampledX.resize(samples);
    r.coveringSources.resize(samples);
    int hit=0,overlap=0,run=0,maxRun=0;
    for(int k=0;k<samples;++k){
        const double x=-targetWidthM/2 + targetWidthM*k/(samples-1);
        r.sampledX[static_cast<std::size_t>(k)]=x;
        int n=0;
        for (const auto& s:r.sources) {
            if (insideNominalBeam(s,{x,audienceDistanceM},fullAngleDeg))++n;
        }
        r.coveringSources[static_cast<std::size_t>(k)]=n;
        hit += n>0;
        overlap += n>1;
        run = n==0 ? run+1 : 0;
        maxRun=std::max(maxRun,run);
    }
    r.fractionCovered=static_cast<double>(hit)/samples;
    r.fractionOverlap=static_cast<double>(overlap)/samples;
    r.maxUncoveredGapM=std::min(targetWidthM,targetWidthM*maxRun/(samples-1));
    return r;
}
} // namespace lah::engineering
