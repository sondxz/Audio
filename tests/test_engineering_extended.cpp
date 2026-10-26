#include "../src/engineering/EngineeringCore.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>

#define REQUIRE(x) do {if(!(x)){std::cerr<<"FAILED line "<<__LINE__<<": "#x<<'\n';std::exit(1);}}while(false)
static bool near(double a,double b,double tolerance=1e-8){return std::abs(a-b)<=tolerance;}
template<class F> void rejects(F action){bool caught=false;try{action();}catch(const std::invalid_argument&){caught=true;}REQUIRE(caught);}
using namespace lah::engineering;

void networkReferences(){
    using N=ResistiveNode;
    // Kirchhoff hand solution: (4+8)||(6+6), 24V. Each branch carries2A;
    // individual drops8,16,12,12V and powers16,32,24,24W.
    const auto mixed=resistiveNetwork(N::parallel({N::series({N::load(4),N::load(8)}),
                                                  N::series({N::load(6),N::load(6)})}),24);
    REQUIRE(near(mixed.equivalentOhms,6) && near(mixed.totalCurrentRms,4) && near(mixed.totalPowerW,96));
    const double voltage[]={8,16,12,12},power[]={16,32,24,24};
    double sum=0;
    for(std::size_t i=0;i<4;++i){REQUIRE(near(mixed.speakers[i].voltageRms,voltage[i]));
        REQUIRE(near(mixed.speakers[i].currentRms,2));REQUIRE(near(mixed.speakers[i].powerW,power[i]));sum+=mixed.speakers[i].powerW;}
    REQUIRE(near(sum,mixed.totalPowerW));
    const auto other=resistiveNetwork(N::series({N::parallel({N::load(4),N::load(4)}),
                                                N::parallel({N::load(8),N::load(8)})}),24);
    REQUIRE(near(other.equivalentOhms,6) && near(other.speakers[0].powerW,16) && near(other.speakers[3].powerW,32));
    // Celestion's published4x8ohm series/parallel example has8ohm total.
    const auto four=resistiveNetwork(N::parallel({N::series({N::load(8),N::load(8)}),N::series({N::load(8),N::load(8)})}),40);
    REQUIRE(near(four.equivalentOhms,8) && near(four.totalPowerW,200));
    for(const auto& speaker:four.speakers)REQUIRE(near(speaker.powerW,50));
    REQUIRE(near(resistiveNetwork(N::series({N::load(8),N::load(8)}),16).totalCurrentRms,1));
    REQUIRE(near(resistiveNetwork(N::parallel({N::load(8),N::load(8)}),8).totalCurrentRms,2));
    REQUIRE(resistiveNetwork(N::load(8),0).totalPowerW==0);
    rejects([]{resistiveNetwork(N::parallel({}),10);});
    rejects([]{resistiveNetwork(N::load(0),10);});
    rejects([]{resistiveNetwork(N::load(8),std::numeric_limits<double>::quiet_NaN());});
    rejects([]{resistiveNetwork(N::parallel(std::vector<N>(129,N::load(8))),10);});
    auto deep=N::load(8);for(int i=0;i<10;++i)deep=N::series({deep,N::load(8)});
    rejects([&]{resistiveNetwork(deep,10);});
}
void amplifierReferences(){
    // Clear-input image regressions are arithmetic references, not validation
    // of an entire third-party tool or unspecified rating conventions.
    const auto voltage=ohmVoltage(70,8);
    REQUIRE(near(voltage.powerW,612.5) && near(voltage.iRms,8.75));
    const auto comparison=ratedPowerComparison(1000,500);
    REQUIRE(near(comparison.powerRatio,2) && near(comparison.powerMarginDb,3.010299956639812));
    const auto study=amplifierHeadroom(100,8,3.010299956639812,40,20000,10,20);
    REQUIRE(near(study.requiredVoltageRms,28.2842712474619));
    REQUIRE(near(study.requiredVoltagePeak,40));
    REQUIRE(near(study.peakHeadroomDb,3.010299956639812));
    REQUIRE(near(study.maximumAveragePowerAtCrestW,200));
    REQUIRE(near(study.requiredSlewVPerUs,5.02654824574367));
    REQUIRE(near(study.maximumSineFrequencyAtClipHz,28134.8848799096,1e-6));
    REQUIRE(near(study.requiredInputVoltageRms,2.82842712474619));
    REQUIRE(!study.peakClipPredicted && !study.slewLimitPredicted);
    const auto limited=amplifierHeadroom(100,8,6.020599913279624,20,20000,1,20);
    REQUIRE(limited.peakClipPredicted && limited.slewLimitPredicted);
    // 1000W into10ohm =>100Vrms,141.421356Vpk, SR17.77153175V/us@20kHz.
    const auto high=amplifierHeadroom(1000,10,3.010299956639812,100,20000,40,32);
    REQUIRE(near(high.requiredSlewVPerUs,17.7715317526335,1e-10));
    rejects([]{amplifierHeadroom(100,8,std::numeric_limits<double>::infinity(),40,20000,10,20);});
    rejects([]{ratedPowerComparison(1000,0);});
}
void limiterReferences(){
    const auto limit=limiterVoltagePlan(100,8,80,20,24);
    REQUIRE(near(limit.speakerVoltageRms,28.2842712474619));
    REQUIRE(near(limit.ampInputVoltageRms,2.82842712474619));
    REQUIRE(near(limit.ampInputVoltagePeak,8));
    REQUIRE(near(limit.rmsThresholdDbu,11.2448658197932));
    REQUIRE(near(limit.peakThresholdSineEquivalentDbu,17.2654657330729));
    REQUIRE(near(limit.rmsThresholdSineReferenceDbfs,-12.7551341802068));
    REQUIRE(near(limit.peakThresholdDbfs,-6.73453426692714));
    const auto post=limiterVoltagePlan(100,8,80,20,24,6);
    REQUIRE(near(post.peakThresholdDbfs,limit.peakThresholdDbfs-6));
    REQUIRE(near(post.rmsThresholdDbu,limit.rmsThresholdDbu)); // physical amp input threshold unchanged
    const auto imageRms=theoreticalRmsThreshold(500,8,32);
    REQUIRE(near(imageRms.thresholdDbu,6.23456586315342));
    // Preserve explicit peak definitions. A3000W INSTANTANEOUS peak across8ohm
    // means154.919Vpk, NOT a3000W sine-RMS rating. Unknown screenshot peak
    // definitions remain pending rather than forcing an unexplained match.
    const auto explicitPeak=limiterVoltagePlan(500,8,peakVoltageFromInstantaneousPower(3000,8),32,24);
    REQUIRE(near(explicitPeak.speakerVoltagePeak,154.919333848297,1e-9));
    REQUIRE(near(explicitPeak.peakThresholdSineEquivalentDbu,11.00577841035));
    REQUIRE(limiterVoltagePlan(1000,8,80,32,24).peakBelowRmsVoltage);
    rejects([]{limiterVoltagePlan(100,8,std::numeric_limits<double>::quiet_NaN(),20,24);});
    rejects([]{peakVoltageFromInstantaneousPower(1000,0);});
}
void gainChainReferences(){
    GainChainInput input;
    input.sourceRmsDbu=0;input.sourceCrestDb=3.010299956639812;
    input.mixerClipSineDbu=20;input.adcFullScaleSineDbu=20;input.dacFullScaleSineDbu=20;
    input.ampVoltageGainDb=20;input.ampClipSineVrms=15.5;input.loadOhms=8;
    const auto chain=gainChain(input);
    REQUIRE(near(chain.mixerPeakHeadroomDb,20) && near(chain.adcPeakDbfs,-20));
    REQUIRE(near(chain.dspOutputPeakDbfs,-20) && near(chain.ampInputRmsDbu,0));
    REQUIRE(near(chain.demandedAmpVrms,7.75) && near(chain.demandedResistivePowerW,7.5078125));
    REQUIRE(near(chain.ampPeakHeadroomDb,6.020599913279624));
    REQUIRE(!chain.mixerClips && !chain.adcClips && !chain.dacClips && !chain.ampClips);
    input.dspGainDb=24;
    const auto clipped=gainChain(input);
    REQUIRE(clipped.dacClips && clipped.ampClips && near(clipped.dspOutputPeakDbfs,4));
    input.sourceCrestDb=std::numeric_limits<double>::quiet_NaN();
    rejects([&]{gainChain(input);});
}
void mainsReferences(){
    // Schneider example5000W,PF0.5 =>10000VA.230V =>43.47826A.
    const auto single=mainsSupply(5000,0.5,230,1);
    REQUIRE(near(single.apparentVa,10000) && near(single.lineCurrentA,43.4782608695652));
    const auto three=mainsSupply(6900,0.9,400,3);
    REQUIRE(near(three.lineCurrentA,11.0658801594678));
    REQUIRE(near(three.phaseNeutralVolts,230.94010767585));
    const auto drop1=voltageDrop(20,230,50,10,1,1);
    REQUIRE(near(drop1.oneConductorOhms,0.0875) && near(drop1.dropVolts,3.5));
    REQUIRE(near(drop1.dropPercent,1.52173913043478) && near(drop1.conductorHeatW,70));
    const auto drop3=voltageDrop(20,400,50,10,1,3);
    REQUIRE(near(drop3.dropVolts,3.03108891324554) && near(drop3.conductorHeatW,105));
    const auto reactive=voltageDrop(three.lineCurrentA,400,50,10,0.9,3,0.0225,0.08);
    REQUIRE(near(reactive.dropVolts,1.97404322523381));
    const auto balanced=assignedPhaseLoads({{2300,1,0},{2300,1,1},{2300,1,2}},230);
    REQUIRE(near(balanced.summedInputCurrentA[0],10) && near(balanced.currentSpreadPercent,0));
    const auto assigned=assignedPhaseLoads({{2300,1,0},{1150,0.5,0},{2300,1,1}},230);
    REQUIRE(near(assigned.summedInputCurrentA[0],20) && near(assigned.summedInputCurrentA[1],10));
    REQUIRE(near(assigned.summedInputCurrentA[2],0) && near(assigned.currentSpreadPercent,100));
    REQUIRE(near(assigned.totalRealPowerW,5750) && near(assigned.maximumPhaseCurrentA,20));
    rejects([]{mainsSupply(1000,0,230,1);});rejects([]{mainsSupply(1000,1,230,2);});
    rejects([]{voltageDrop(10,230,20,0,1,1);});
    rejects([]{assignedPhaseLoads({{1000,1,3}},230);});
    rejects([]{assignedPhaseLoads({{std::numeric_limits<double>::quiet_NaN(),1,0}},230);});
}
void converterReferences(){
    const auto zeroDbu=convertAudioLevel(0,AudioLevelUnit::Dbu,24);
    REQUIRE(near(zeroDbu.voltsRms,0.775) && near(zeroDbu.levelDbfsSineReference,-24));
    REQUIRE(near(zeroDbu.levelDbv,-2.21396594987379));
    const auto zeroDbv=convertAudioLevel(0,AudioLevelUnit::Dbv);
    REQUIRE(near(zeroDbv.voltsRms,1) && near(zeroDbv.levelDbu,2.21396594987379));
    REQUIRE(near(convertAudioLevel(-20,AudioLevelUnit::DbfsSineReference,20).levelDbu,0));
    REQUIRE(near(convertAudioLevel(2.82842712474619,AudioLevelUnit::VoltsPeakToPeak).voltsRms,1));
    REQUIRE(near(convertAudioLevel(1.414213562373095,AudioLevelUnit::VoltsPeak).voltsRms,1));
    rejects([]{convertAudioLevel(0,AudioLevelUnit::VoltsRms);});
    rejects([]{convertAudioLevel(std::numeric_limits<double>::infinity(),AudioLevelUnit::Dbu);});
    rejects([]{convertAudioLevel(1,static_cast<AudioLevelUnit>(999));});
}
int main(){networkReferences();amplifierReferences();limiterReferences();gainChainReferences();mainsReferences();converterReferences();
    std::cout<<"PASS: engineering known-reference resistive networks, voltage/power, headroom/slew, explicit RMS/peak thresholds, gain-chain clipping, mains/phase/drop, level conversion and invalid inputs; math only\n";}
