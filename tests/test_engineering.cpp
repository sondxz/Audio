#include "../src/engineering/EngineeringCore.h"
#include "../src/engineering/CoverageCore.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <cstdlib>

#define CHECK(x) do {if(!(x)){std::cerr<<"FAIL at line "<<__LINE__<<": "<<#x<<'\n';std::exit(1);}}while(false)
static bool near(double a,double b,double tolerance) {return std::abs(a-b)<tolerance;}
using namespace lah::engineering;
int main(){
    const auto ohm=ohmPower(500,8);
    CHECK(near(ohm.vRms,63.2455532,1e-6));
    CHECK(near(ohm.iRms,7.90569415,1e-6));
    CHECK(near(ohm.powerW,500,1e-9));
    CHECK(near(voltsFromDbu(dbu(70)),70,1e-10));
    const auto limit=theoreticalRmsThreshold(500,8,32);
    CHECK(near(limit.thresholdDbu,6.23,.01));
    CHECK(near(limit.nominalSpeakerVrms,std::sqrt(4000.0),1e-8));
    const auto spl=idealSpl(98,500,20,1,false);
    CHECK(near(spl.atOneM,124.9897,.002));
    CHECK(near(spl.estimateDb,98.9691,.003));
    CHECK(near(idealSpl(98,500,20,4,false).arrayGainDb,6.0206,.0002));
    CHECK(near(idealSpl(98,500,20,4,true).arrayGainDb,12.0412,.0002));
    const auto cb=cable(8,20,2.5,2000,70);
    CHECK(near(cb.cableLoopOhms,0.28,1e-9));
    CHECK(near(cb.effectiveDamping,8/.284,1e-8));
    CHECK(cb.voltageLossDb<0);
    CHECK(cb.cableHeatW>0 && cb.deliveredPowerW>0);
    CHECK(near(speedOfSound(20),343.2,1e-10));
    CHECK(speedOfSound(34)>speedOfSound(22));
    CHECK(arrivalDriftMs(60,34,22)>0);
    const auto phase=phaseForDelay(85,2,20);
    CHECK(near(phase.phaseDegrees,61.2,1e-7));
    CHECK(near(phase.wavelengthM,343.2/85,1e-7));
    // Published ISO 9613-1 Table 1 octave-band coefficients @20C 50% RH:
    // 1000 Hz ~4.66 dB/km and 8000 Hz ~105.29 dB/km (exact freq).
    CHECK(near(iso9613AirAbsorptionDbPerM(1000,20,50)*1000,4.66,.04));
    CHECK(near(iso9613AirAbsorptionDbPerM(8000,20,50)*1000,105.29,.12));
    CHECK(near(totalAirLossDb(8000,20,50,50),5.2645,.01));
    // Independent National Physical Laboratory ISO 9613-1 validation cells.
    CHECK(near(iso9613AirAbsorptionDbPerM(50.12,-20,10),0.000589,0.000005));
    CHECK(near(iso9613AirAbsorptionDbPerM(10000,20,15),0.267237,0.0002));
    auto autoFill=calculateFill(24,5,60,.15);
    CHECK(autoFill.sources.size()>1 && autoFill.sources.size()<=80);
    CHECK(autoFill.fractionCovered>.99);
    CHECK(autoFill.maxUncoveredGapM<0.1);
    const auto tiny=calculateFill(2,10,100,.15);
    CHECK(tiny.sources.size()==1);
    CHECK(tiny.fractionCovered>.99);
    const auto missing=calculateFill(60,2,20,.1,2);
    CHECK(missing.fractionCovered<.3 && missing.maxUncoveredGapM>10);
    const auto arc=calculateFill(24,5,60,.15,6,1);
    CHECK(arc.sources.size()==6);
    CHECK(arc.sources.front().y<0 && arc.sources[3].y<0);
    CHECK(near(beamIntersectionAtAudience(0,0,5,45),5,1e-10));
    bool thrown=false;try{(void)ohmPower(10,0);}catch(const std::invalid_argument&){thrown=true;}
    CHECK(thrown);
    thrown=false;try{(void)iso9613AirAbsorptionDbPerM(20000,20,50);}catch(const std::invalid_argument&){thrown=true;}
    CHECK(thrown);
    std::cout<<"PASS: engineering power/voltage, ideal SPL, limiter math, cable, ISO 9613, phase, temperature, front fill geometry\n";
}
