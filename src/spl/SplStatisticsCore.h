#pragma once
// Offline/incremental power-domain sound-level statistics for successive,
// NON-OVERLAPPING windows. Does not claim IEC 61672 conformance.
#include "CalibratedSplCore.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace lah::spl {
struct Statistics {
    bool valid=false;
    double elapsedSeconds=0, leqA=0,leqC=0,leqZ=0;
    double fastA=0,slowA=0;
    std::size_t windows=0;
};
class SoundLevelStatistics {
public:
    void clear() noexcept {energy={};filtered={};seconds=0;count=0;}
    bool add(const Reading& current) noexcept {
        const double dt=current.sampleSeconds;
        if(!current.valid || !std::isfinite(dt) || dt<=0 || dt>10.0 ||
           !std::isfinite(current.dbA) || !std::isfinite(current.dbC) ||
           !std::isfinite(current.dbZ)) return false;
        const auto powers=std::array<double,3>{std::pow(10.0,current.dbA/10.0),
                                              std::pow(10.0,current.dbC/10.0),
                                              std::pow(10.0,current.dbZ/10.0)};
        auto nextEnergy=energy;
        for(std::size_t i=0;i<3;++i) {
            if(!std::isfinite(powers[i]))return false;
            nextEnergy[i]+=powers[i]*dt;
            if(!std::isfinite(nextEnergy[i]))return false;
        }
        const double nextSeconds=seconds+dt;
        if(!std::isfinite(nextSeconds))return false;
        const double aFast=std::exp(-dt/0.125);
        const double aSlow=std::exp(-dt/1.0);
        if(count==0){filtered[0]=powers[0];filtered[1]=powers[0];}
        else {
            filtered[0]=aFast*filtered[0]+(1.0-aFast)*powers[0];
            filtered[1]=aSlow*filtered[1]+(1.0-aSlow)*powers[0];
        }
        // Commit only after every channel has passed. A rejected C/Z reading
        // must not silently add A-energy to an otherwise unchanged history.
        energy=nextEnergy;seconds=nextSeconds; ++count;
        return true;
    }
    Statistics current() const noexcept {
        Statistics s;
        if(count==0 || seconds<=0)return s;
        s.valid=true;s.elapsedSeconds=seconds;s.windows=count;
        const auto db=[](double power) {return 10.0*std::log10(std::max(power,1e-30));};
        s.leqA=db(energy[0]/seconds);s.leqC=db(energy[1]/seconds);s.leqZ=db(energy[2]/seconds);
        s.fastA=db(filtered[0]);s.slowA=db(filtered[1]);
        return s;
    }
private:
    std::array<double,3> energy{};
    std::array<double,2> filtered{};
    double seconds=0;
    std::size_t count=0;
};
} // namespace lah::spl
