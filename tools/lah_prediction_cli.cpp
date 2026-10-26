// QA-only batch bridge. Input/output on stdin/stdout avoids locale/path issues.
#include "../src/engineering/PhaseCore.h"
#include "../src/simulation/SubArrayCore.h"
#include <iomanip>
#include <iostream>
#include <locale>
#include <string>

int main(){
    std::cin.imbue(std::locale::classic());std::cout.imbue(std::locale::classic());
    std::cout<<std::setprecision(17);
    try{
        char kind;
        while(std::cin>>kind){
            int count=0;
            if(!(std::cin>>count))throw std::invalid_argument("Missing source count");
            if(kind=='P'){
                if(count<2 || count>16)throw std::invalid_argument("Invalid phase count");
                double time=0;if(!(std::cin>>time))throw std::invalid_argument("Missing time");
                std::vector<lah::engineering::phase::Source> sources(static_cast<std::size_t>(count));
                for(auto& s:sources)if(!(std::cin>>s.frequencyHz>>s.phaseDegrees>>s.delayMs>>s.levelDb>>s.inverted>>s.enabled))
                    throw std::invalid_argument("Invalid phase input");
                const auto result=lah::engineering::phase::combine(sources,time);
                std::cout<<result.instantaneousPhasor.real()<<','<<result.instantaneousPhasor.imag()<<','
                    <<result.upperPeakBound<<','<<result.commonFrequency<<','<<result.steadyRms<<','<<result.phaseDefined<<'\n';
            }else if(kind=='S'){
                if(count<1 || count>128)throw std::invalid_argument("Invalid sub count");
                lah::simulation::Parameters p;lah::simulation::Point3 point;
                if(!(std::cin>>p.speedOfSound>>p.frequencyHz>>point.x>>point.y>>point.z))throw std::invalid_argument("Invalid sub parameters");
                std::vector<lah::simulation::Sub> sources(static_cast<std::size_t>(count));
                for(auto& s:sources)if(!(std::cin>>s.position.x>>s.position.y>>s.position.z>>s.gainDb>>s.delayMs>>s.invert>>s.enabled))
                    throw std::invalid_argument("Invalid sub source");
                if(!lah::simulation::validScene(sources,point,p))throw std::invalid_argument("Invalid scene");
                const auto pressure=lah::simulation::pressure(sources,point,p);
                std::cout<<pressure.real()<<','<<pressure.imag()<<','<<lah::simulation::relativeDb(sources,point,p)<<'\n';
            }else throw std::invalid_argument("Unknown case kind");
        }
        if(!std::cin.eof())throw std::invalid_argument("Invalid trailing input");
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
    return 0;
}
