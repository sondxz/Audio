#include "EngineeringPanel.h"
#include "../engineering/EngineeringCore.h"
#include <algorithm>
#include <cmath>

namespace lah::ui {
namespace {
const juce::Colour bg {0xff101419}, panel {0xff181e25}, accent {0xff22cfae};
struct FieldSpec {const char* label; double min,max,step,start;};
using Specs = std::array<FieldSpec,5>;
const Specs& specsFor(int id) {
    static const Specs specs[] = {
        {{{"Power RMS (W)",1,10000,1,500},{"Nominal R (ohm)",1,32,.5,8},{"-",0,1,1,0},{"-",0,1,1,0},{"-",0,1,1,0}}},
        {{{"Sensitivity 1W/1m (dB SPL)",60,120,.5,98},{"Power per loudspeaker (W)",1,10000,1,500},{"Listener distance (m)",1,200,1,20},{"Number of loudspeakers",1,64,1,1},{"Coherent? 0=no / 1=yes",0,1,1,0}}},
        {{{"Speaker continuous rating (W)",1,10000,1,500},{"Nominal load R (ohm)",1,32,.5,8},{"AMP voltage gain (dB)",0,60,.5,32},{"-",0,1,1,0},{"-",0,1,1,0}}},
        {{{"Nominal load R (ohm)",1,32,.5,8},{"Cable one-way length (m)",0,150,.5,20},{"Copper area (mm2)",.5,16,.5,2.5},{"AMP damping factor",1,5000,1,2000},{"AMP open-circuit Vrms",1,200,1,70}}},
        {{{"Frequency (Hz)",50,10000,10,8000},{"Temperature (C)",-20,50,.5,20},{"Relative humidity (%)",10,100,1,50},{"Travel distance (m)",0,300,1,50},{"Air pressure (kPa)",80,110,.5,101.325}}},
        {{{"Frequency (Hz)",10,2000,1,85},{"Relative delay (ms)",-20,20,.1,2},{"Temperature (C)",-20,50,.5,20},{"Distance (m)",0,200,1,60},{"Show temperature (C)",-20,50,.5,34}}}
    };
    return specs[std::clamp(id,1,6)-1];
}
juce::String f(double v, int places=2){return juce::String(v,places);}
std::vector<FieldSpec> extraSpecs(int id){
    if(id==7)return {{"Supply Vrms",.1,500,.1,24},{"R1 (ohm)",.5,32,.5,4},{"R2 (ohm)",.5,32,.5,8},{"R3 (ohm)",.5,32,.5,6},{"R4 (ohm)",.5,32,.5,6},{"Wiring 1=S 2=P 3=(S)||(S) 4=(P)+(P)",1,4,1,3}};
    if(id==8)return {{"Average speaker power (W)",1,10000,1,500},{"Load (ohm)",1,32,.5,8},{"Signal crest factor (dB)",0,30,.1,12},{"Amplifier sine clip (Vrms)",1,300,.1,100},{"Sine frequency (Hz)",1,30000,1,20000},{"Available slew (V/us)",.1,200,.1,40},{"Amplifier voltage gain (dB)",0,60,.1,32},{"Speaker continuous rating (W)",1,10000,1,500}};
    if(id==9)return {{"Continuous speaker power (W)",1,10000,1,500},{"Load (ohm)",1,32,.5,8},{"Explicit terminal limit (Vpk)",1,500,.001,154.919},{"Amplifier voltage gain (dB)",0,60,.1,32},{"DAC full-scale sine (dBu)",-10,40,.1,24},{"Gain AFTER limiter (dB)",-40,40,.1,0}};
    if(id==10)return {{"Source RMS (dBu)",-80,30,.1,4},{"Signal crest factor (dB)",0,30,.1,12},{"Mixer sine clip (dBu)",-10,40,.1,24},{"ADC full-scale sine (dBu)",-10,40,.1,24},{"DSP gain / attenuation (dB)",-60,40,.1,0},{"DAC full-scale sine (dBu)",-10,40,.1,24},{"Amplifier voltage gain (dB)",0,60,.1,32},{"Amplifier sine clip (Vrms)",1,300,.1,70},{"Load (ohm)",1,32,.5,8}};
    if(id==11)return {{"ELECTRICAL INPUT power (W)",0,100000,10,5000},{"Lagging power factor",.05,1,.01,.9},{"Supply: 1ph L-N / 3ph L-L (V)",100,480,1,230},{"Supply phases: 1 or balanced 3",1,3,2,1},{"Cable one-way (m)",0,200,.5,25},{"Copper cross-section (mm2)",.5,95,.5,6}};
    return {{"Unit 0=dBu 1=dBV 2=Vrms 3=Vpk 4=Vpp 5=dBFS",0,5,1,0},{"Input value",-200,1000,.01,0},{"Full-scale sine reference (dBu)",-10,40,.1,24}};
}
}
EngineeringPanel::EngineeringPanel(){
    setOpaque(true);
    addAndMakeVisible(calculator);
    for(const auto& name : {"Ohm / RMS Power", "Speaker / SPL estimate",
                           "Limiter - theoretical RMS", "Cable / damping factor",
                           "Atmospheric absorption", "Phase / thermal drift", "Series / parallel speakers",
                           "Amplifier headroom / slew", "RMS / peak voltage limits", "Gain chain / clipping",
                           "Electrical supply / voltage drop", "Audio unit converter"})
        calculator.addItem(juce::String(name),calculator.getNumItems()+1);
    calculator.onChange=[this]{chooseCalculator();};
    addAndMakeVisible(title); addAndMakeVisible(warning); addAndMakeVisible(result);addAndMakeVisible(details);
    title.setColour(juce::Label::textColourId,juce::Colours::white);
    title.setFont(juce::FontOptions(18.0f));
    title.setMinimumHorizontalScale(1.0f);
    warning.setColour(juce::Label::textColourId,juce::Colour(0xffffc882));
    warning.setJustificationType(juce::Justification::centredLeft);
    warning.setFont(juce::FontOptions(13.0f));
    warning.setMinimumHorizontalScale(1.0f);
    result.setReadOnly(true);
    result.setMultiLine(true);
    result.setScrollbarsShown(true);
    result.setColour(juce::TextEditor::backgroundColourId,panel);
    result.setColour(juce::TextEditor::textColourId,juce::Colours::white);
    result.setFont(juce::FontOptions(15.0f));
    result.setBorder(juce::BorderSize<int>(14));
    details.setClickingTogglesState(true);
    details.onClick=[this]{resized();repaint();};
    for(std::size_t i=0;i<metricNames.size();++i){
        addAndMakeVisible(metricNames[i]);addAndMakeVisible(metricValues[i]);
        metricNames[i].setColour(juce::Label::textColourId,juce::Colour(0xffa6bacd));
        metricNames[i].setFont(juce::FontOptions(13.0f));
        metricNames[i].setMinimumHorizontalScale(1.0f);
        metricValues[i].setColour(juce::Label::textColourId,juce::Colour(0xffe7f7f2));
        metricValues[i].setFont(juce::FontOptions(18.0f));
        metricValues[i].setMinimumHorizontalScale(1.0f);
    }
    for(int i=0;i<9;++i){
        auto& slider=values[static_cast<std::size_t>(i)];
        auto& label=labels[static_cast<std::size_t>(i)];
        addAndMakeVisible(slider);addAndMakeVisible(label);
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight,false,90,28);
        slider.setColour(juce::Slider::thumbColourId,accent);
        slider.onValueChange=[this]{recalculate();};
        label.setColour(juce::Label::textColourId,juce::Colour(0xffd2dfeb));
        label.setFont(juce::FontOptions(13.0f));
        label.setMinimumHorizontalScale(1.0f);
    }
    calculator.setSelectedId(1,juce::sendNotificationSync);
}
void EngineeringPanel::setValidationScenario(int scenario){calculator.setSelectedId(std::clamp(scenario,0,11)+1,juce::sendNotificationSync);}
void EngineeringPanel::chooseCalculator(){
    kind=calculator.getSelectedId();
    const auto& specs=specsFor(kind);
    const auto extra=extraSpecs(kind);
    for(std::size_t i=0;i<values.size();++i){
        const auto s=kind<=6?(i<5?specs[i]:FieldSpec{"-",0,1,1,0}):(i<extra.size()?extra[i]:FieldSpec{"-",0,1,1,0});
        const bool visible=s.label[0]!='-';
        labels[i].setVisible(visible);values[i].setVisible(visible);
        if(visible){
            labels[i].setText(s.label,juce::dontSendNotification);labels[i].setTooltip(s.label);
            values[i].setRange(s.min,s.max,s.step);
            values[i].setValue(s.start,juce::dontSendNotification);
        }
    }
    const juce::String names[]={juce::String::fromUTF8("OHM'S LAW  •  RESISTIVE LOAD"), juce::String::fromUTF8("SPL FREE FIELD  •  ESTIMATE"),
        juce::String::fromUTF8("SPEAKER LIMIT  •  THEORETICAL RMS"), "CABLE LOOP + EFFECTIVE DAMPING",
        juce::String::fromUTF8("ISO 9613-1 FORMULA  •  AIR ABSORPTION"), "PHASE / AIR TEMPERATURE SHIFT"};
    title.setText(kind<=6?names[std::clamp(kind,1,6)-1]:calculator.getText().toUpperCase(),juce::dontSendNotification);
    resized();recalculate();
}
void EngineeringPanel::recalculate(){
    if(kind>=7){recalculateExtended();return;}
    using namespace engineering;
    const auto a=values[0].getValue(),b=values[1].getValue(),c=values[2].getValue(),
               d=values[3].getValue(),e=values[4].getValue();
    juce::String text, note;
    try {
        if(kind==1){
            const auto r=ohmPower(a,b);
            text="P = V^2/R = I^2R\n\n";
            text+="Voltage RMS: "+f(r.vRms)+" V\nPeak (sine): "+f(r.vPeak)+" V\n";
            text+="Current RMS: "+f(r.iRms)+" A\nPower: "+f(r.powerW,1)+" W\n";
            text+="Voltage dBu: "+f(r.levelDbu)+" dBu";
            note="Purely resistive load, sinusoidal RMS. Loudspeaker impedance changes with frequency.";
        }else if(kind==2){
            const auto r=idealSpl(a,b,c,static_cast<int>(d),e>0.5);
            text="Single at 1m: "+f(r.atOneM)+" dB SPL (estimate)\n";
            text+="Distance loss: "+f(r.distanceLossDb)+" dB\n";
            text+="Ideal array gain: +"+f(r.arrayGainDb)+" dB\n";
            text+="Result at listener: "+f(r.estimateDb)+" dB SPL (idealized)";
            note="Not a calibrated meter! No coverage, phase, thermal compression, air/ground/room loss.";
        }else if(kind==3){
            const auto r=theoreticalRmsThreshold(a,b,c);
            text="Speaker electrical V(RMS): "+f(r.nominalSpeakerVrms)+" V\n";
            text+="Speaker sine V(peak): "+f(r.voltagePeak)+" V\n";
            text+="DSP input RMS V: "+f(r.dspInputVrms)+" V\n";
            text+="Calculated nominal threshold: "+f(r.thresholdDbu)+" dBu\n";
            text+="Formula: 20log10(sqrt(P*R)/0.775) - amp voltage gain";
            note="NOT a safe limiter setting. Verify speaker rating, measured amp gain, crest factor, DSP scale and manufacturer parameters. Never auto-apply.";
        }else if(kind==4){
            const auto r=cable(a,b,c,d,e);
            text="Cable loop DC resistance: "+f(r.cableLoopOhms,4)+" ohm\n";
            text+="Equivalent amp source: "+f(r.sourceOhms,4)+" ohm\n";
            text+="System damping factor: "+f(r.effectiveDamping,1)+"\n";
            text+="Voltage attenuation: "+f(r.voltageLossDb)+" dB\n";
            text+="Delivered speaker power: "+f(r.deliveredPowerW,1)+" W\n";
            text+="Cable heat (estimated): "+f(r.cableHeatW,1)+" W";
            note="DC copper at about 20C, 2-wire round trip; ignores reactance and temperature rise. NOT cable/amp safety certification.";
        }else if(kind==5){
            const auto alpha=iso9613AirAbsorptionDbPerM(a,b,c,e);
            text="Air absorption coefficient: "+f(alpha*1000,3)+" dB/km\n";
            text+="Loss at "+f(d,0)+" m: "+f(alpha*d,3)+" dB\n";
            text+="Speed of sound (temp model): "+f(speedOfSound(b),2)+" m/s\n";
            text+="Geometric loss 1m -> "+f(std::max(1.0,d),0)+"m: "+f(-20*std::log10(std::max(1.0,d)))+" dB\n";
            note="ISO 9613-1 formula (tone loss ONLY); does not include reflections, terrain or loudspeaker directionality.";
        }else if(kind==6){
            const auto p=phaseForDelay(a,b,c);
            text="Wavelength: "+f(p.wavelengthM,3)+" m\n";
            text+="Quarter wavelength: "+f(p.quarterWaveM,3)+" m\n";
            text+="Unwrapped propagation lag: "+f(p.phaseDegrees,2)+" degrees\n";
            text+="Signed signal phase (delay retards): "+f(std::remainder(-p.phaseDegrees,360.0),2)+" degrees\n";
            text+="Speed of sound: "+f(speedOfSound(c),2)+" m/s\n";
            text+="Thermal travel-time drift at "+f(d,0)+"m: "+f(arrivalDriftMs(d,c,e),3)+" ms";
            note="Pure propagation/relative delay math. Does not determine correct sub/top alignment without measured transfer function.";
        }
    }catch(const std::exception& err){text="Invalid inputs: "+juce::String(err.what());note="Correct the inputs before using any result.";}
    setResults(text,note);
}
void EngineeringPanel::setResults(const juce::String& text,const juce::String& note){
    result.setText(text,false);
    warning.setText(note,juce::dontSendNotification);warning.setTooltip(note);
    metricCount=0;
    for(const auto& raw:juce::StringArray::fromLines(text)){
        const auto line=raw.trim();
        if(line.startsWithIgnoreCase("FORMULA:")||line.startsWithIgnoreCase("EXAMPLE:")||line.startsWithIgnoreCase("APPLICATION:")||line.startsWithIgnoreCase("Reference"))break;
        const int separator=line.indexOfChar(':');
        if(separator<0||metricCount>=static_cast<int>(metricNames.size()))continue;
        const auto index=static_cast<std::size_t>(metricCount++);
        metricNames[index].setText(line.substring(0,separator),juce::dontSendNotification);
        metricValues[index].setText(line.substring(separator+1).trim(),juce::dontSendNotification);
        const bool attention=line.startsWithIgnoreCase("WARNING")||line.startsWithIgnoreCase("Invalid")||
            (line.containsIgnoreCase("headroom")&&metricValues[index].getText().startsWithChar('-'))||
            (metricValues[index].getText()=="YES"&&(line.containsIgnoreCase("clipping")||line.containsIgnoreCase("exceeded")));
        metricValues[index].setColour(juce::Label::textColourId,attention?juce::Colour(0xffffad78):juce::Colour(0xffe7f7f2));
        metricNames[index].setTooltip(line);metricValues[index].setTooltip(line);
    }
    resized();repaint();
}
void EngineeringPanel::paint(juce::Graphics& g){
    g.fillAll(bg);
    g.setColour(panel);g.fillRoundedRectangle(inputBounds,6.0f);
    g.setColour(accent.withAlpha(.2f));g.drawRoundedRectangle(inputBounds,6.0f,1.0f);
    g.setColour(juce::Colour(0xff92a9bc));g.setFont(13.0f);
    g.drawText("INPUTS",inputBounds.reduced(14).removeFromTop(20),juce::Justification::centredLeft);
    g.drawText(details.getToggleState()?"FORMULAS, EXAMPLES & APPLICATION":"CALCULATED RESULTS",resultBounds.withY(57).withHeight(23),juce::Justification::centredLeft);
    if(!details.getToggleState())for(int i=0;i<metricCount;++i){
        g.setColour(panel);g.fillRoundedRectangle(metricBounds[static_cast<std::size_t>(i)],5.0f);
        g.setColour(accent.withAlpha(.17f));g.drawRoundedRectangle(metricBounds[static_cast<std::size_t>(i)],5.0f,1.0f);
        g.setColour(accent.withAlpha(.7f));
        const auto bounds=metricBounds[static_cast<std::size_t>(i)];
        g.fillRoundedRectangle(bounds.getX(),bounds.getY()+14,3,bounds.getHeight()-28,1.5f);
    }
    if(!absorptionBounds.isEmpty()) {
        const auto region=absorptionBounds;
        g.setColour(panel);g.fillRoundedRectangle(region,7.0f);
        juce::Path path;
        double maxLevel=0;
        for(int i=0;i<=80;++i){
            const double hz=50*std::pow(10000.0/50.0,i/80.0);
            maxLevel=std::max(maxLevel,engineering::iso9613AirAbsorptionDbPerM(hz,values[1].getValue(),values[2].getValue(),values[4].getValue())*values[3].getValue());
        }
        for(int i=0;i<=80;++i){
            const double hz=50*std::pow(10000.0/50.0,i/80.0);
            const double v=engineering::iso9613AirAbsorptionDbPerM(hz,values[1].getValue(),values[2].getValue(),values[4].getValue())*values[3].getValue();
            const float x=region.getX()+52+(region.getWidth()-72)*i/80.0f;
            const float y=region.getBottom()-30-static_cast<float>(maxLevel>0 ? v/maxLevel:0)*(region.getHeight()-75);
            if(i==0)path.startNewSubPath(x,y);else path.lineTo(x,y);
        }
        g.setColour(accent);g.strokePath(path,juce::PathStrokeType(2.2f));
        g.setColour(juce::Colours::white);g.setFont(13.0f);
        g.drawText("Air attenuation / dB",region.reduced(14).removeFromTop(22),juce::Justification::left);
        g.setColour(juce::Colour(0xff9bb0c2));
        g.drawText("50 Hz",region.withX(region.getX()+40).withY(region.getBottom()-25).withWidth(64).withHeight(20),juce::Justification::centred);
        g.drawText("10 kHz",region.withX(region.getRight()-75).withY(region.getBottom()-25).withWidth(64).withHeight(20),juce::Justification::centred);
        g.drawText(f(maxLevel,1),region.withY(region.getY()+40).withWidth(47).withHeight(20),juce::Justification::centredRight);
        g.drawText("0",region.withY(region.getBottom()-42).withWidth(47).withHeight(20),juce::Justification::centredRight);
    }
}
void EngineeringPanel::resized(){
    const auto w=getWidth(),h=getHeight();
    const int inputWidth=std::clamp(w/3,330,400),rightX=inputWidth+28,rightWidth=std::max(180,w-rightX-16);
    calculator.setBounds(16,12,std::clamp(w-220,180,440),34);
    details.setBounds(std::max(16,w-198),12,182,34);
    title.setBounds(16,55,inputWidth,25);
    const int contentHeight=std::max(310,h-162);
    inputBounds={16.0f,88.0f,static_cast<float>(inputWidth),static_cast<float>(contentHeight)};
    resultBounds={static_cast<float>(rightX),88.0f,static_cast<float>(rightWidth),static_cast<float>(contentHeight)};
    int visible=0;for(auto& value:values)if(value.isVisible())++visible;
    const int rowHeight=std::clamp((contentHeight-42)/std::max(1,visible),48,60);
    int row=0;
    for(std::size_t i=0;i<values.size();++i)if(values[i].isVisible()){
        const int y=123+row++*rowHeight;
        labels[i].setBounds(28,y,inputWidth-24,20);
        values[i].setBounds(26,y+20,inputWidth-22,29);
    }
    const bool showDetails=details.getToggleState();
    result.setVisible(showDetails);result.setBounds(resultBounds.toNearestInt());
    const int columns=rightWidth>=480?2:1;
    const int rows=(metricCount+columns-1)/columns;
    const bool plot=kind==5&&!showDetails&&contentHeight>=390;
    const int metricArea=plot?std::min(230,contentHeight-170):contentHeight;
    const int metricHeight=std::clamp((metricArea-10*std::max(0,rows-1))/std::max(1,rows),58,94);
    const int metricWidth=(rightWidth-10*(columns-1))/columns;
    for(std::size_t i=0;i<metricNames.size();++i){
        const bool show=!showDetails&&static_cast<int>(i)<metricCount;
        metricNames[i].setVisible(show);metricValues[i].setVisible(show);
        if(!show)continue;
        const int x=rightX+(static_cast<int>(i)%columns)*(metricWidth+10),y=88+(static_cast<int>(i)/columns)*(metricHeight+10);
        metricBounds[i]={static_cast<float>(x),static_cast<float>(y),static_cast<float>(metricWidth),static_cast<float>(metricHeight)};
        const int nameHeight=metricHeight>=82?34:22;
        metricNames[i].setBounds(x+14,y+7,metricWidth-26,nameHeight);
        metricValues[i].setBounds(x+14,y+nameHeight+9,metricWidth-26,std::max(24,metricHeight-nameHeight-15));
        const auto fontSize=std::clamp(static_cast<float>(metricWidth-26)/std::max(1.0f,static_cast<float>(metricValues[i].getText().length())*.56f),13.0f,18.0f);
        metricValues[i].setFont(juce::FontOptions(fontSize));
    }
    absorptionBounds=plot?juce::Rectangle<float>(static_cast<float>(rightX),static_cast<float>(88+metricArea+12),static_cast<float>(rightWidth),static_cast<float>(contentHeight-metricArea-12)):juce::Rectangle<float>{};
    warning.setBounds(18,h-62,std::max(180,w-36),54);
}
} // namespace lah::ui
