#include "EngineeringPanel.h"
#include "../engineering/EngineeringCore.h"

namespace lah::ui {
void EngineeringPanel::recalculateExtended(){
    using namespace engineering;
    const auto value=[this](int i){return values[static_cast<std::size_t>(i)].getValue();};
    const auto fmt=[](double x){return juce::String(x,3);};
    juce::String text,note;
    try{
        if(kind==7){
            using N=ResistiveNode;
            const auto a=N::load(value(1)),b=N::load(value(2)),c=N::load(value(3)),d=N::load(value(4));
            const int wiring=static_cast<int>(value(5));
            const auto network=wiring==1?N::series({a,b,c,d}):wiring==2?N::parallel({a,b,c,d}):
                wiring==3?N::parallel({N::series({a,b}),N::series({c,d})}):N::series({N::parallel({a,b}),N::parallel({c,d})});
            const auto r=resistiveNetwork(network,value(0));
            text="Equivalent resistance: "+fmt(r.equivalentOhms)+" ohm\nTotal: "+fmt(r.totalCurrentRms)+" Arms / "+fmt(r.totalPowerW)+" W\n\n";
            int index=0;for(const auto& s:r.speakers)text+="Speaker "+juce::String(++index)+": "+fmt(s.voltageRms)+" Vrms / "+fmt(s.currentRms)+" Arms / "+fmt(s.powerW)+" W\n";
            text+="\nFORMULA: series R=sum(R); parallel 1/R=sum(1/R); V=IR; P=VI.\nEXAMPLE: (4+8)||(6+6) at 24V = 6 ohm, 4A, 96W.\nAPPLICATION: compare each speaker's electrical share with its rating.\nReference: Celestion Speaker Wiring Configurations.";
            note="Resistive model. Loudspeaker impedance varies with frequency; this does not certify an amplifier load.";
        }else if(kind==8){
            const auto r=amplifierHeadroom(value(0),value(1),value(2),value(3),value(4),value(5),value(6));
            const auto rating=ratedPowerComparison(value(3)*value(3)/value(1),value(7));
            text="Required: "+fmt(r.requiredVoltageRms)+" Vrms / "+fmt(r.requiredVoltagePeak)+" Vpk\nAvailable: "+fmt(r.availableVoltagePeak)+" Vpk\nPeak headroom: "+fmt(r.peakHeadroomDb)+" dB\nSine slew required: "+fmt(r.requiredSlewVPerUs)+" V/us\nAmp input required: "+fmt(r.requiredInputVoltageRms)+" Vrms\nMax average power at stated crest: "+fmt(r.maximumAveragePowerAtCrestW)+" W\nAmp sine rating / speaker rating: "+fmt(rating.powerRatio)+"x / "+fmt(rating.powerMarginDb)+" dB\n";
            text+=juce::String("Peak clipping predicted: ")+(r.peakClipPredicted?"YES":"NO")+"\nSlew demand exceeded: "+(r.slewLimitPredicted?"YES":"NO");
            text+="\n\nFORMULA: Vpk=Vrms*10^(crest/20); sine SR=2*pi*f*Vpk/1e6.\nEXAMPLE: 100Vrms sine at 20kHz requires 17.772 V/us.\nAPPLICATION: inspect voltage headroom and sine slew demands separately.\nReferences: Analog Devices MT-045; TI slew-rate documentation.";
            note="User supplied crest factor, amplifier clip and slew data. A power ratio alone does not establish safe matching.";
        }else if(kind==9){
            const auto r=limiterVoltagePlan(value(0),value(1),value(2),value(3),value(4),value(5));
            text="Continuous speaker voltage: "+fmt(r.speakerVoltageRms)+" Vrms\nExplicit speaker peak: "+fmt(r.speakerVoltagePeak)+" Vpk\nAmp input RMS threshold: "+fmt(r.rmsThresholdDbu)+" dBu\nPeak threshold, sine-equivalent dBu: "+fmt(r.peakThresholdSineEquivalentDbu)+" dBu\nDSP RMS sine-reference threshold: "+fmt(r.rmsThresholdSineReferenceDbfs)+" dBFS\nDSP peak amplitude threshold: "+fmt(r.peakThresholdDbfs)+" dBFS\n";
            if(r.peakBelowRmsVoltage)text+="WARNING: peak voltage is below continuous RMS voltage.\n";
            text+="\nFORMULA: Vrms=sqrt(Pcontinuous*R); input V=terminal V / amp gain. Peak dBu uses Vpk/sqrt(2).\nEXAMPLE: 500W, 8ohm, gain32dB -> 6.235dBu RMS.\nAPPLICATION: verify actual amplifier gain and DSP full-scale reference. Attack/release, ratio, look-ahead and thermal/mechanical models are separate.\nReference: QSC PLD RMS/peak voltage limiters.";
            note="Electrical threshold calculation only. Does not set a limiter or validate speaker thermal/mechanical protection.";
        }else if(kind==10){
            GainChainInput input;input.sourceRmsDbu=value(0);input.sourceCrestDb=value(1);input.mixerClipSineDbu=value(2);
            input.adcFullScaleSineDbu=value(3);input.dspGainDb=value(4);input.dacFullScaleSineDbu=value(5);
            input.ampVoltageGainDb=value(6);input.ampClipSineVrms=value(7);input.loadOhms=value(8);
            const auto r=gainChain(input);
            text="Mixer peak headroom: "+fmt(r.mixerPeakHeadroomDb)+" dB\nADC peak: "+fmt(r.adcPeakDbfs)+" dBFS\nDSP / DAC demanded peak: "+fmt(r.dspOutputPeakDbfs)+" dBFS\nAmp input RMS: "+fmt(r.ampInputRmsDbu)+" dBu\nDemanded output: "+fmt(r.demandedAmpVrms)+" Vrms / "+fmt(r.demandedAmpVpeak)+" Vpk\nDemanded resistive power: "+fmt(r.demandedResistivePowerW)+" W\nAmp peak headroom: "+fmt(r.ampPeakHeadroomDb)+" dB\nMinimum headroom: "+fmt(r.minimumHeadroomDb)+" dB\n";
            text+="\nFORMULA: digital peak = RMS dBu - full-scale sine dBu + crest - 3.0103dB + DSP gain.\nEXAMPLE: 0dBu sine into +20dBu full-scale = -20dBFS peak.\nAPPLICATION: negative headroom identifies an exceeded stage. Displayed voltages are demanded linear levels before clipping. Noise floor is not modeled.\nReference: Rane Pro Audio Reference, decibels/headroom.";
            note="Headroom follows user calibration. These controls model a chain; they do not change live audio gains.";
        }else if(kind==11){
            const int phases=static_cast<int>(value(3));
            const auto r=mainsSupply(value(0),value(1),value(2),phases);
            const auto drop=voltageDrop(r.lineCurrentA,value(2),value(4),value(5),value(1),phases);
            text="Apparent power: "+fmt(r.apparentVa)+" VA\nLine current: "+fmt(r.lineCurrentA)+" A\nPhase-neutral voltage: "+fmt(r.phaseNeutralVolts)+" V\nCopper voltage drop: "+fmt(drop.dropVolts)+" V / "+fmt(drop.dropPercent)+"%\nReceiving voltage estimate: "+fmt(drop.receivingVoltage)+" V\nConductor heat: "+fmt(drop.conductorHeatW)+" W\n";
            text+="\nFORMULA: I1=P/(V*PF); balanced I3=P/(sqrt(3)*VLL*PF). Copper R=rho*L/A, rho=0.0175 at approx20C; reactance omitted.\nEXAMPLE: 5000W, PF0.5, 230V single-phase = 43.478A.\nAPPLICATION: enter ELECTRICAL consumption, not amplifier speaker watts. A three-phase result assumes balance; harmonics and neutral current are not modeled.\nReference: Schneider Electrical Installation Guide, power demand/voltage drop.";
            note="CB and wire ampacity selection remains pending installation conditions, derating, fault protection and local requirements.";
        }else if(kind==12){
            const auto r=convertAudioLevel(value(1),static_cast<AudioLevelUnit>(static_cast<int>(value(0))),value(2));
            text="RMS: "+fmt(r.voltsRms)+" V\nSine peak: "+fmt(r.voltsPeak)+" V\nSine peak-to-peak: "+fmt(r.voltsPeakToPeak)+" V\ndBu: "+fmt(r.levelDbu)+"\ndBV: "+fmt(r.levelDbv)+"\nSine-reference dBFS: "+fmt(r.levelDbfsSineReference)+"\n";
            text+="\nFORMULA: dBu=20log10(Vrms/0.775); dBV=20log10(Vrms); sine Vpk=sqrt(2)*Vrms.\nEXAMPLE: 0dBu = 0.775Vrms = -2.214dBV.\nAPPLICATION: enter actual analog full-scale calibration when converting dBFS. Peak conversions assume a sine wave.\nReference: Rane Pro Audio Reference, dBu/dBV.";
            note="dBFS conversion requires a known analog full-scale reference. It is not inferred from the mixer model.";
        }
    }catch(const std::exception& e){text="Invalid inputs: "+juce::String::fromUTF8(e.what());note="Correct the inputs before using the result.";}
    setResults(text,note);
}
}
