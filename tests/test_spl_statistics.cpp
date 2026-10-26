#include "../src/spl/SplStatisticsCore.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#define REQ(x) do{if(!(x)){std::cerr<<"FAIL line "<<__LINE__<<": "<<#x<<'\n';std::exit(1);}}while(false)
int main(){
    lah::spl::SoundLevelStatistics acc;
    REQ(!acc.current().valid);
    lah::spl::Reading r{};
    REQ(!acc.add(r));
    r.valid=true;r.dbA=80;r.dbC=82;r.dbZ=85;r.sampleSeconds=1.0;
    REQ(acc.add(r));REQ(acc.add(r));
    auto s=acc.current();
    REQ(s.valid);REQ(s.windows==2);REQ(std::abs(s.elapsedSeconds-2.0)<1e-10);
    REQ(std::abs(s.leqA-80)<1e-10 && std::abs(s.leqC-82)<1e-10 && std::abs(s.leqZ-85)<1e-10);
    r.dbA=90;r.dbC=90;r.dbZ=90;
    REQ(acc.add(r));s=acc.current();
    const double averagePower=(2*std::pow(10.0,8)+std::pow(10.0,9))/3.0;
    REQ(std::abs(s.leqA-10*std::log10(averagePower))<1e-10);
    REQ(s.fastA>s.slowA && s.slowA>s.leqA);
    r.dbA=std::numeric_limits<double>::quiet_NaN();
    REQ(!acc.add(r));REQ(acc.current().windows==3);
    const auto before=acc.current();
    r.dbA=100;r.dbC=10000;r.dbZ=90; // finite dB, but power conversion overflows
    REQ(!acc.add(r));
    s=acc.current();
    REQ(s.windows==before.windows && s.elapsedSeconds==before.elapsedSeconds);
    REQ(s.leqA==before.leqA && s.leqC==before.leqC && s.leqZ==before.leqZ);
    REQ(s.fastA==before.fastA && s.slowA==before.slowA);
    acc.clear();REQ(!acc.current().valid);
    std::cout<<"PASS: Leq energy-domain integration, weighting channels, F/S block smoothing, rejected invalid windows\n";
}
