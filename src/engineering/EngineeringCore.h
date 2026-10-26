#pragma once

// Offline engineering calculators. NOT called by the realtime audio callback.
// All voltage/power calculations assume a sinusoid into a PURELY RESISTIVE load.
// This is NOT a speaker-protection system or a substitute for manufacturer data.
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <utility>
#include <vector>

namespace lah::engineering {
constexpr double pi = 3.14159265358979323846;
inline void check(double x, double low, double high, const char* message) {
    if (!std::isfinite(x) || x < low || x > high) throw std::invalid_argument(message);
}
inline double db(double ratio) { return 20.0 * std::log10(ratio); }
inline double dbu(double vRms) { check(vRms, 1.0e-12, 1.0e9, "Voltage must be positive"); return db(vRms / 0.775); }
inline double voltsFromDbu(double valueDbu) { check(valueDbu, -100, 100, "dBu out of bounds"); return 0.775 * std::pow(10.0, valueDbu / 20.0); }

struct OhmResult { double vRms{}, vPeak{}, iRms{}, powerW{}, levelDbu{}; };
inline OhmResult ohmVoltage(double voltageRms,double resistanceOhms) {
    check(voltageRms,0.001,1000,"Voltage out of range");check(resistanceOhms,0.1,10000,"Resistance out of range");
    return {voltageRms,std::sqrt(2.0)*voltageRms,voltageRms/resistanceOhms,
            voltageRms*voltageRms/resistanceOhms,dbu(voltageRms)};
}
inline OhmResult ohmPower(double powerW, double resistanceOhms) {
    check(powerW, 0.001, 100000, "Power out of range");
    check(resistanceOhms, 0.1, 10000, "Resistance out of range");
    const auto v = std::sqrt(powerW * resistanceOhms);
    return {v, std::sqrt(2.0)*v, v/resistanceOhms, powerW, dbu(v)};
}

struct SplResult {double atOneM{}, distanceLossDb{}, arrayGainDb{}, estimateDb{};};
// 1W/1m sensitivity is manufacturer dependent. Coherent vs incoherent sum are
// limiting cases, NOT necessarily what a spatially distributed array achieves.
inline SplResult idealSpl(double sensitivityDb, double powerW, double distanceM,
                          int nSpeakers, bool coherent) {
    check(sensitivityDb, 40, 140, "Sensitivity out of range");
    check(powerW, 0.001, 100000, "Power out of range");
    check(distanceM, 1.0, 2000, "Distance must be >=1m (free-field model)");
    if (nSpeakers < 1 || nSpeakers > 128) throw std::invalid_argument("Invalid speaker count");
    const double oneM = sensitivityDb + 10.0 * std::log10(powerW);
    const double loss = -20.0 * std::log10(distanceM);
    const double array = (coherent ? 20.0 : 10.0) * std::log10(static_cast<double>(nSpeakers));
    return {oneM, loss, array, oneM+loss+array};
}

struct LimiterResult {double nominalSpeakerVrms{}, dspInputVrms{}, thresholdDbu{}, voltagePeak{};};
// MATHEMATICAL RMS VOLTAGE EQUIVALENT, not attack/release/true-peak protection.
inline LimiterResult theoreticalRmsThreshold(double continuousPowerW,
                                             double nominalOhms, double ampGainDb) {
    check(ampGainDb, 0, 80, "Amplifier voltage gain out of range");
    const auto o = ohmPower(continuousPowerW, nominalOhms);
    const auto input = o.vRms / std::pow(10.0, ampGainDb/20.0);
    return {o.vRms, input, dbu(input), o.vPeak};
}

struct CableResult {double cableLoopOhms{}, sourceOhms{}, effectiveDamping{},
                    voltageLossDb{}, deliveredPowerW{}, cableHeatW{};};
inline CableResult cable(double loadOhms, double oneWayLengthM, double areaMm2,
                         double amplifierDf, double ampOpenCircuitVrms,
                         double copperRho=0.0175) {
    check(loadOhms, 0.5, 1000, "Nominal load out of range");
    check(oneWayLengthM, 0, 1000, "Cable length out of range");
    check(areaMm2, 0.1, 100, "Cross section out of range");
    check(amplifierDf, 1, 100000, "Amplifier DF out of range");
    check(ampOpenCircuitVrms, 0.001, 500, "Open circuit amp voltage out of range");
    check(copperRho, 0.01, 0.05, "Copper resistivity out of range");
    const double rc = copperRho * 2.0 * oneWayLengthM / areaMm2;
    // DF at the load impedance being used; if spec defined at another
    // impedance, sourceOhms must be recalculated externally.
    const double ra = loadOhms / amplifierDf;
    const double current = ampOpenCircuitVrms / (loadOhms + rc + ra);
    const double ratio = loadOhms / (loadOhms + rc + ra);
    return {rc, ra, loadOhms / (rc + ra), db(ratio),
            current*current*loadOhms, current*current*rc};
}

inline double speedOfSound(double temperatureC) {
    check(temperatureC, -20, 50, "Temperature out of range");
    // Reference ideal-air approximation: 343.2 m/s at 20 C.
    return 343.2 * std::sqrt((temperatureC+273.15)/293.15);
}
inline double millisecondsPerMeter(double temperatureC) {
    return 1000.0/speedOfSound(temperatureC);
}
inline double arrivalDriftMs(double meters, double dayTempC, double showTempC) {
    check(meters, 0, 10000, "Distance out of range");
    return meters * 1000.0*(1.0/speedOfSound(showTempC)-1.0/speedOfSound(dayTempC));
}
struct PhaseResult {double wavelengthM{}, phaseDegrees{}, delayMs{}, quarterWaveM{};};
inline PhaseResult phaseForDelay(double frequencyHz, double delayMs, double tempC) {
    check(frequencyHz, 1, 20000, "Frequency out of range");
    check(delayMs, -1000, 1000, "Delay out of range");
    const auto wavelength = speedOfSound(tempC) / frequencyHz;
    return {wavelength, frequencyHz * delayMs * 0.36, delayMs, wavelength/4.0};
}

// ISO 9613-1:1993 equation-based estimate of tone absorption, dB/m.
// 50..10000Hz, -20..50C, 10..100% RH and 101.325 kPa default, not ISO certified.
inline double iso9613AirAbsorptionDbPerM(double frequencyHz, double tempC,
                                          double relativeHumidityPercent,
                                          double pressureKPa=101.325) {
    check(frequencyHz, 50, 10000, "Frequency must be 50..10000Hz");
    check(tempC, -20, 50, "Temperature must be -20..50C");
    check(relativeHumidityPercent, 10, 100, "Humidity must be 10..100% RH");
    check(pressureKPa, 50, 120, "Pressure out of range");
    constexpr double refT=293.15, tripleT=273.16, refP=101.325;
    const double t = tempC+273.15;
    const double relativePressure=pressureKPa/refP;
    const double sat = refP * std::pow(10.0, -6.8346*std::pow(tripleT/t,1.261)+4.6151);
    const double h = relativeHumidityPercent*sat/pressureKPa; // molar %
    const double frO = relativePressure*(24.0+4.04e4*h*(0.02+h)/(0.391+h));
    const double frN = relativePressure*std::pow(t/refT,-0.5)*
        (9.0+280.0*h*std::exp(-4.170*(std::pow(t/refT,-1.0/3.0)-1.0)));
    const double f2 = frequencyHz*frequencyHz;
    return 8.686*f2*(1.84e-11 / relativePressure*std::sqrt(t/refT)+
        std::pow(t/refT,-2.5)*(
            0.01275*std::exp(-2239.1/t)/(frO + f2/frO) +
            0.1068*std::exp(-3352.0/t)/(frN + f2/frN)));
}
inline double totalAirLossDb(double freqHz, double tempC, double rhPercent, double distanceM) {
    check(distanceM, 0, 10000, "Distance out of range");
    return iso9613AirAbsorptionDbPerM(freqHz,tempC,rhPercent)*distanceM;
}

// Extended calculators remain independent of devices and the realtime engine.
// Primary formula references (not third-party source code):
// Celestion: https://celestion.com/blog/speaker-wiring-configurations/
// Rane: https://www.ranecommercial.com/legacy/pdf/ranenotes/Signal_Processing_Fundamentals.pdf
// Analog Devices: https://www.analog.com/en/resources/faqs/faq_opamp_slewrate.html
// QSC PLD manual: https://www.qsc.com/resource-files/productresources/amp/pld/q_amp_pld_usermanual.pdf
// Schneider: https://www.electrical-installation.org/enwiki/Definition_of_reactive_power
// https://www.electrical-installation.org/enwiki/Calculation_of_voltage_drop_in_steady_load_conditions

enum class ResistiveConnection {Load,Series,Parallel};
struct ResistiveNode {
    ResistiveConnection connection=ResistiveConnection::Load;
    double ohms=8.0;
    std::vector<ResistiveNode> children;
    static ResistiveNode load(double resistance) {return {ResistiveConnection::Load,resistance,{}};}
    static ResistiveNode series(std::vector<ResistiveNode> nodes) {return {ResistiveConnection::Series,0,std::move(nodes)};}
    static ResistiveNode parallel(std::vector<ResistiveNode> nodes) {return {ResistiveConnection::Parallel,0,std::move(nodes)};}
};
struct SpeakerElectricalShare {double ohms{},voltageRms{},currentRms{},powerW{};};
struct ResistiveNetworkResult {
    double equivalentOhms{},supplyVoltageRms{},totalCurrentRms{},totalPowerW{};
    std::vector<SpeakerElectricalShare> speakers; // depth-first input order
};
namespace detail {
struct SolvedResistance {ResistiveConnection connection;double resistance;std::vector<SolvedResistance> children;};
inline SolvedResistance solveResistance(const ResistiveNode& node,int depth,int& leaves) {
    if(depth>8)throw std::invalid_argument("Network nesting exceeds 8 levels");
    if(node.connection==ResistiveConnection::Load) {
        if(!node.children.empty() || ++leaves>128)throw std::invalid_argument("Invalid network leaf/count");
        check(node.ohms,0.1,10000,"Each resistive speaker load must be 0.1..10000 ohm");
        return {node.connection,node.ohms,{}};
    }
    if(node.connection!=ResistiveConnection::Series && node.connection!=ResistiveConnection::Parallel)
        throw std::invalid_argument("Unknown network connection");
    if(node.children.size()<2 || node.children.size()>128)throw std::invalid_argument("A connection needs 2..128 children");
    SolvedResistance solved{node.connection,0,{}};
    for(const auto& child:node.children) {
        auto part=solveResistance(child,depth+1,leaves);
        solved.resistance+=node.connection==ResistiveConnection::Series?part.resistance:1.0/part.resistance;
        solved.children.push_back(std::move(part));
    }
    if(node.connection==ResistiveConnection::Parallel)solved.resistance=1.0/solved.resistance;
    return solved;
}
inline void distributeResistance(const SolvedResistance& node,double voltage,
                                 std::vector<SpeakerElectricalShare>& shares) {
    const double current=voltage/node.resistance;
    if(node.connection==ResistiveConnection::Load) {
        shares.push_back({node.resistance,voltage,current,voltage*current});return;
    }
    for(const auto& child:node.children)
        distributeResistance(child,node.connection==ResistiveConnection::Series?current*child.resistance:voltage,shares);
}
}
inline ResistiveNetworkResult resistiveNetwork(const ResistiveNode& network,double supplyVoltageRms) {
    check(supplyVoltageRms,0,1000,"Network voltage must be 0..1000 Vrms");
    int leaves=0;
    const auto solved=detail::solveResistance(network,0,leaves);
    ResistiveNetworkResult r;
    r.equivalentOhms=solved.resistance;r.supplyVoltageRms=supplyVoltageRms;
    r.totalCurrentRms=supplyVoltageRms/solved.resistance;r.totalPowerW=supplyVoltageRms*r.totalCurrentRms;
    r.speakers.reserve(static_cast<std::size_t>(leaves));
    detail::distributeResistance(solved,supplyVoltageRms,r.speakers);
    return r;
}

struct AmplifierHeadroomResult {
    double requiredVoltageRms{},requiredVoltagePeak{},availableVoltagePeak{},peakHeadroomDb{};
    double maximumAveragePowerAtCrestW{},requiredSlewVPerUs{},availableSlewVPerUs{};
    double maximumSineFrequencyAtClipHz{},requiredInputVoltageRms{};
    bool peakClipPredicted{},slewLimitPredicted{};
};
struct RatedPowerComparison {double powerRatio{},powerMarginDb{},voltageRatio{};};
inline RatedPowerComparison ratedPowerComparison(double amplifierRatedW,double speakerRatedW) {
    check(amplifierRatedW,0.001,1000000,"Amplifier rated power out of range");
    check(speakerRatedW,0.001,1000000,"Speaker rated power out of range");
    const double ratio=amplifierRatedW/speakerRatedW;
    return {ratio,10.0*std::log10(ratio),std::sqrt(ratio)};
}
// Clip Vrms is the amplifier's maximum undistorted SINE-RMS voltage at this
// load. Crest factor is 20log10(Vpk/Vrms), independent of a music power label.
inline AmplifierHeadroomResult amplifierHeadroom(double averagePowerW,double loadOhms,double crestFactorDb,
                                                 double sineClipVrms,double frequencyHz,double availableSlewVPerUs,
                                                 double ampVoltageGainDb) {
    check(averagePowerW,0.000001,100000,"Average electrical power out of range");
    check(loadOhms,0.1,1000,"Load out of range");check(crestFactorDb,0,40,"Crest factor must be 0..40 dB");
    check(sineClipVrms,0.001,1000,"Sine clip voltage out of range");check(frequencyHz,1,100000,"Frequency out of range");
    check(availableSlewVPerUs,0.001,10000,"Slew rate must be positive V/us");check(ampVoltageGainDb,0,80,"Amp gain out of range");
    AmplifierHeadroomResult r;
    const double crest=std::pow(10.0,crestFactorDb/20.0),gain=std::pow(10.0,ampVoltageGainDb/20.0);
    r.requiredVoltageRms=std::sqrt(averagePowerW*loadOhms);r.requiredVoltagePeak=r.requiredVoltageRms*crest;
    r.availableVoltagePeak=std::sqrt(2.0)*sineClipVrms;
    r.peakHeadroomDb=db(r.availableVoltagePeak/r.requiredVoltagePeak);
    r.maximumAveragePowerAtCrestW=std::pow(r.availableVoltagePeak/crest,2)/loadOhms;
    r.requiredSlewVPerUs=2*pi*frequencyHz*r.requiredVoltagePeak/1.0e6;
    r.availableSlewVPerUs=availableSlewVPerUs;
    r.maximumSineFrequencyAtClipHz=availableSlewVPerUs*1.0e6/(2*pi*r.availableVoltagePeak);
    r.requiredInputVoltageRms=r.requiredVoltageRms/gain;
    r.peakClipPredicted=r.requiredVoltagePeak>r.availableVoltagePeak;
    r.slewLimitPredicted=r.requiredSlewVPerUs>availableSlewVPerUs;
    return r;
}

struct LimiterVoltagePlan {
    double speakerVoltageRms{},speakerVoltagePeak{},ampInputVoltageRms{},ampInputVoltagePeak{};
    double rmsThresholdDbu{},peakThresholdSineEquivalentDbu{},rmsThresholdSineReferenceDbfs{},peakThresholdDbfs{};
    bool peakBelowRmsVoltage{};
};
inline double peakVoltageFromInstantaneousPower(double instantaneousPeakW,double loadOhms) {
    check(instantaneousPeakW,0.001,1000000,"Instantaneous peak power out of range");
    check(loadOhms,0.1,1000,"Load out of range");return std::sqrt(instantaneousPeakW*loadOhms);
}
// The peak input is an explicit terminal Vpk limit, not a continuous/program
// watt rating. Both thresholds are electrical equivalents, not limiter timing.
// dBFS mapping requires the USER'S full-scale sine-RMS dBu calibration.
inline LimiterVoltagePlan limiterVoltagePlan(double continuousPowerW,double loadOhms,double terminalPeakVolts,
                                              double ampVoltageGainDb,double dacFullScaleSineDbu,double postLimiterGainDb=0) {
    const auto rms=ohmPower(continuousPowerW,loadOhms);
    check(terminalPeakVolts,0.001,10000,"Peak voltage must be supplied as Vpk");
    check(ampVoltageGainDb,0,80,"Amp gain out of range");check(postLimiterGainDb,-80,60,"Post-limiter gain out of range");
    const double fullScaleRms=voltsFromDbu(dacFullScaleSineDbu),gain=std::pow(10.0,ampVoltageGainDb/20.0);
    LimiterVoltagePlan r;
    r.speakerVoltageRms=rms.vRms;r.speakerVoltagePeak=terminalPeakVolts;
    r.ampInputVoltageRms=rms.vRms/gain;r.ampInputVoltagePeak=terminalPeakVolts/gain;
    r.rmsThresholdDbu=dbu(r.ampInputVoltageRms);
    r.peakThresholdSineEquivalentDbu=dbu(r.ampInputVoltagePeak/std::sqrt(2.0));
    r.rmsThresholdSineReferenceDbfs=db(r.ampInputVoltageRms/fullScaleRms)-postLimiterGainDb;
    r.peakThresholdDbfs=db(r.ampInputVoltagePeak/(std::sqrt(2.0)*fullScaleRms))-postLimiterGainDb;
    r.peakBelowRmsVoltage=terminalPeakVolts<rms.vRms;
    return r;
}

struct GainChainInput {
    double sourceRmsDbu=4,sourceCrestDb=12,mixerClipSineDbu=24,adcFullScaleSineDbu=24;
    double dspGainDb=0,dacFullScaleSineDbu=24,ampVoltageGainDb=32,ampClipSineVrms=70,loadOhms=8;
};
struct GainChainResult {
    double mixerPeakHeadroomDb{},adcPeakDbfs{},dspOutputPeakDbfs{},ampInputRmsDbu{};
    double demandedAmpVrms{},demandedAmpVpeak{},demandedResistivePowerW{},ampPeakHeadroomDb{},minimumHeadroomDb{};
    bool mixerClips{},adcClips{},dacClips{},ampClips{};
};
inline GainChainResult gainChain(const GainChainInput& in) {
    check(in.sourceRmsDbu,-100,60,"Source RMS dBu out of range");check(in.sourceCrestDb,0,40,"Crest factor out of range");
    check(in.mixerClipSineDbu,-20,60,"Mixer sine clip dBu out of range");
    check(in.adcFullScaleSineDbu,-20,60,"ADC full-scale sine dBu out of range");
    check(in.dacFullScaleSineDbu,-20,60,"DAC full-scale sine dBu out of range");
    check(in.dspGainDb,-100,60,"DSP gain out of range");check(in.ampVoltageGainDb,0,80,"Amp gain out of range");
    check(in.ampClipSineVrms,0.001,1000,"Amp sine clip Vrms out of range");check(in.loadOhms,0.1,1000,"Load out of range");
    constexpr double sineCrestDb=3.010299956639812;
    GainChainResult r;
    const double crestAboveSine=in.sourceCrestDb-sineCrestDb;
    r.mixerPeakHeadroomDb=in.mixerClipSineDbu-in.sourceRmsDbu-crestAboveSine;
    r.adcPeakDbfs=in.sourceRmsDbu-in.adcFullScaleSineDbu+crestAboveSine;
    r.dspOutputPeakDbfs=r.adcPeakDbfs+in.dspGainDb;
    r.ampInputRmsDbu=in.sourceRmsDbu-in.adcFullScaleSineDbu+in.dspGainDb+in.dacFullScaleSineDbu;
    // Intermediate dBu can exceed converter capabilities; this is the demanded
    // LINEAR value used to expose clipping, never a prediction after clipping.
    r.demandedAmpVrms=0.775*std::pow(10.0,(r.ampInputRmsDbu+in.ampVoltageGainDb)/20.0);
    r.demandedAmpVpeak=r.demandedAmpVrms*std::pow(10.0,in.sourceCrestDb/20.0);
    r.demandedResistivePowerW=r.demandedAmpVrms*r.demandedAmpVrms/in.loadOhms;
    r.ampPeakHeadroomDb=db(std::sqrt(2.0)*in.ampClipSineVrms/r.demandedAmpVpeak);
    r.minimumHeadroomDb=std::min({r.mixerPeakHeadroomDb,-r.adcPeakDbfs,-r.dspOutputPeakDbfs,r.ampPeakHeadroomDb});
    r.mixerClips=r.mixerPeakHeadroomDb<0;r.adcClips=r.adcPeakDbfs>0;
    r.dacClips=r.dspOutputPeakDbfs>0;r.ampClips=r.ampPeakHeadroomDb<0;
    return r;
}

struct MainsSupplyResult {double apparentVa{},lineCurrentA{},phaseNeutralVolts{};};
inline MainsSupplyResult mainsSupply(double realInputPowerW,double powerFactor,double supplyVoltage,int phases) {
    check(realInputPowerW,0,10000000,"Electrical input power out of range");check(powerFactor,0.05,1,"Power factor must be 0.05..1");
    check(supplyVoltage,1,1000,"Supply voltage out of range");
    if(phases!=1 && phases!=3)throw std::invalid_argument("Supply must be single phase or balanced three phase");
    const double factor=phases==3?std::sqrt(3.0):1.0;
    return {realInputPowerW/powerFactor,realInputPowerW/(factor*supplyVoltage*powerFactor),supplyVoltage/factor};
}
struct VoltageDropResult {double dropVolts{},dropPercent{},receivingVoltage{},oneConductorOhms{},conductorHeatW{};};
// Constant-current, sinusoidal lagging-PF approximation. For THREE phases the
// load MUST be balanced and voltage is L-L. rho is user supplied at conductor
// temperature; defaults to copper near20C, not installed cable ampacity.
inline VoltageDropResult voltageDrop(double currentA,double voltage,double oneWayM,double areaMm2,double powerFactor,
                                      int phases,double rhoOhmMm2PerM=0.0175,double reactanceOhmPerKm=0) {
    check(currentA,0,10000,"Current out of range");check(voltage,1,1000,"Voltage out of range");
    check(oneWayM,0,10000,"Cable length out of range");check(areaMm2,0.1,1000,"Conductor area out of range");
    check(powerFactor,0.05,1,"Lagging displacement PF out of range");check(rhoOhmMm2PerM,0.01,0.1,"Resistivity out of range");
    check(reactanceOhmPerKm,0,10,"Cable reactance out of range");
    if(phases!=1 && phases!=3)throw std::invalid_argument("Voltage drop supports 1 or balanced3 phases only");
    const double resistance=rhoOhmMm2PerM*oneWayM/areaMm2;
    const double reactance=reactanceOhmPerKm*oneWayM/1000.0;
    const double drop=(phases==3?std::sqrt(3.0):2.0)*currentA*
                      (resistance*powerFactor+reactance*std::sqrt(1.0-powerFactor*powerFactor));
    return {drop,drop*100.0/voltage,voltage-drop,resistance,(phases==3?3.0:2.0)*currentA*currentA*resistance};
}
struct AssignedMainsLoad {double realInputPowerW{},powerFactor{1};int phase{};}; // phase0..2
struct PhaseLoadResult {
    std::array<double,3> powerW{},summedInputCurrentA{},apparentVa{};
    double totalRealPowerW{},maximumPhaseCurrentA{},currentSpreadPercent{};
};
inline PhaseLoadResult assignedPhaseLoads(const std::vector<AssignedMainsLoad>& loads,double phaseNeutralVolts) {
    check(phaseNeutralVolts,1,1000,"Phase-neutral voltage out of range");
    if(loads.empty() || loads.size()>128)throw std::invalid_argument("Enter 1..128 assigned loads");
    PhaseLoadResult r;
    for(const auto& load:loads) {
        if(load.phase<0 || load.phase>2)throw std::invalid_argument("Load phase must be 0,1,2");
        const auto single=mainsSupply(load.realInputPowerW,load.powerFactor,phaseNeutralVolts,1);
        const auto phase=static_cast<std::size_t>(load.phase);
        r.powerW[phase]+=load.realInputPowerW;r.apparentVa[phase]+=single.apparentVa;
        r.summedInputCurrentA[phase]+=single.lineCurrentA;r.totalRealPowerW+=load.realInputPowerW;
    }
    r.maximumPhaseCurrentA=*std::max_element(r.summedInputCurrentA.begin(),r.summedInputCurrentA.end());
    const double mean=(r.summedInputCurrentA[0]+r.summedInputCurrentA[1]+r.summedInputCurrentA[2])/3.0;
    if(mean>0)for(double current:r.summedInputCurrentA)r.currentSpreadPercent=std::max(r.currentSpreadPercent,std::abs(current-mean)*100.0/mean);
    // These are sums of appliance RMS current estimates (conservative bound),
    // not a neutral-current/harmonic/shared-neutral unbalanced feeder solution.
    return r;
}

enum class AudioLevelUnit {Dbu,Dbv,VoltsRms,VoltsPeak,VoltsPeakToPeak,DbfsSineReference};
struct AudioLevelResult {double voltsRms{},voltsPeak{},voltsPeakToPeak{},levelDbu{},levelDbv{},levelDbfsSineReference{};};
inline AudioLevelResult convertAudioLevel(double value,AudioLevelUnit unit,double fullScaleSineDbu=24) {
    check(value,-1000000,1000000,"Audio level must be finite and in range");
    const double fs=voltsFromDbu(fullScaleSineDbu);
    double rms=0;
    switch(unit) {
        case AudioLevelUnit::Dbu:rms=voltsFromDbu(value);break;
        case AudioLevelUnit::Dbv:check(value,-100,100,"dBV out of range");rms=std::pow(10.0,value/20.0);break;
        case AudioLevelUnit::VoltsRms:rms=value;break;
        case AudioLevelUnit::VoltsPeak:rms=value/std::sqrt(2.0);break;
        case AudioLevelUnit::VoltsPeakToPeak:rms=value/(2.0*std::sqrt(2.0));break;
        case AudioLevelUnit::DbfsSineReference:check(value,-200,40,"dBFS out of range");rms=fs*std::pow(10.0,value/20.0);break;
        default:throw std::invalid_argument("Unknown audio level unit");
    }
    check(rms,1.0e-12,1.0e9,"Voltage must be positive; peak conversions assume a sine wave");
    return {rms,rms*std::sqrt(2.0),rms*2.0*std::sqrt(2.0),dbu(rms),db(rms),db(rms/fs)};
}
} // namespace lah::engineering
