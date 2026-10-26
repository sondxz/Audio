#include "PhasePanel.h"
#include <algorithm>
#include <cmath>

namespace lah::ui {
namespace {
const juce::Colour background{0xff0d1015},card{0xff171d26},grid{0xff35404d},teal{0xff27d8b7};
juce::String number(double value,int digits=3){return juce::String(value,digits);}
void setupSlider(juce::Slider& slider,double low,double high,double step,double value){
    slider.setRange(low,high,step);slider.setValue(value,juce::dontSendNotification);
    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight,false,90,26);
}
}
PhasePanel::PhasePanel(){
    setOpaque(true);
    for(auto* label:{&title,&notice,&countLabel,&sourceLabel,&summary,&selectedDetails,&cursorLabel,
        &conversionTitle,&conversionResult,&phaseLabel,&delayLabel,&frequencyLabel,&branchLabel}){
        addAndMakeVisible(*label);label->setColour(juce::Label::textColourId,juce::Colour(0xffd1dce6));
        label->setFont(juce::FontOptions(13.0f));label->setMinimumHorizontalScale(1.0f);
    }
    title.setText("PHASE ENGINE / SINE SUMMATION LAB",juce::dontSendNotification);
    title.setFont(juce::FontOptions(19.0f));
    notice.setText(juce::String::fromUTF8("MÔ PHỎNG TOÁN HỌC / KHÔNG PHÁT AUDIO. Không đo coherence, SPL hay phase của hệ loa thực."),juce::dontSendNotification);
    notice.setColour(juce::Label::textColourId,juce::Colour(0xffffc783));
    countLabel.setText("SOURCE COUNT",juce::dontSendNotification);
    sourceLabel.setText("EDIT SOURCE",juce::dontSendNotification);
    for(auto* combo:{&presets,&count,&sourceSelect,&cycles,&conversionMode})addAndMakeVisible(*combo);
    presets.addItem("2 sources / in phase",1);presets.addItem("2 sources / cancellation",2);
    presets.addItem("2 sources / +90 deg",3);presets.addItem("2 sources / +quarter-cycle delay",4);
    presets.addItem("100 + 125 Hz / mixed frequency",5);
    presets.setTextWhenNothingSelected("CUSTOM SOURCE SETTINGS");
    for(std::size_t size=2;size<=engineering::phase::maximumSources;++size)
        count.addItem(juce::String(static_cast<int>(size)),static_cast<int>(size));
    cycles.addItem("1 cycle window",1);cycles.addItem("2 cycle window",2);cycles.addItem("4 cycle window",4);
    cycles.setSelectedId(2,juce::dontSendNotification);
    addAndMakeVisible(linked);addAndMakeVisible(enabled);addAndMakeVisible(polarity);addAndMakeVisible(converterToggle);
    converterToggle.setClickingTogglesState(true);
    converterToggle.onClick=[this]{
        converterToggle.setButtonText(converterToggle.getToggleState()?"RETURN TO PHASE LAB":"PHASE / DELAY CONVERTER");
        resized();repaint();
    };
    linked.setToggleState(true,juce::dontSendNotification);
    const char* const names[]{"Frequency (Hz)","Phase setting (deg; + = lead)","Signed delay (ms; + = retard)","Relative level (dB; 0 dB = peak 1)"};
    const double low[]{0.1,-720.0,-1000.0,-80.0},high[]{20000.0,720.0,1000.0,24.0},step[]{0.1,0.1,0.001,0.01};
    for(std::size_t index=0;index<sourceValues.size();++index){
        auto& slider=sourceValues[index];auto& label=sourceLabels[index];
        addAndMakeVisible(slider);addAndMakeVisible(label);label.setText(names[index],juce::dontSendNotification);
        label.setColour(juce::Label::textColourId,juce::Colour(0xffd1dce6));
        label.setFont(juce::FontOptions(13.0f));label.setMinimumHorizontalScale(1.0f);
        setupSlider(slider,low[index],high[index],step[index],index==0?100.0:0.0);
        if(index==0)slider.setSkewFactorFromMidPoint(1000.0);
        slider.onValueChange=[this,index]{
            if(selected>=sources.size())return;
            auto& source=sources[selected];const auto value=sourceValues[index].getValue();
            if(index==0){
                source.frequencyHz=value;
                if(linked.getToggleState())for(auto& item:sources)item.frequencyHz=value;
            }else if(index==1)source.phaseDegrees=value;
            else if(index==2)source.delayMs=value;
            else source.levelDb=value;
            presets.setSelectedId(0,juce::dontSendNotification);
            recalculate();
        };
    }
    presets.onChange=[this]{setTestScenario(presets.getSelectedId()-1);};
    count.onChange=[this]{
        const auto size=static_cast<std::size_t>(std::clamp(count.getSelectedId(),2,16));
        const auto frequency=sources.empty()?100.0:sources[selected].frequencyHz;
        const auto oldSize=sources.size();sources.resize(size);
        if(linked.getToggleState())for(std::size_t index=oldSize;index<size;++index)sources[index].frequencyHz=frequency;
        selected=std::min(selected,size-1);presets.setSelectedId(0,juce::dontSendNotification);
        rebuildSources();refreshSelected();recalculate();
    };
    sourceSelect.onChange=[this]{
        selected=static_cast<std::size_t>(std::max(0,sourceSelect.getSelectedId()-1));refreshSelected();recalculate();
    };
    linked.onClick=[this]{
        if(linked.getToggleState())for(auto& source:sources)source.frequencyHz=sources[selected].frequencyHz;
        presets.setSelectedId(0,juce::dontSendNotification);
        recalculate();
    };
    enabled.onClick=[this]{sources[selected].enabled=enabled.getToggleState();presets.setSelectedId(0,juce::dontSendNotification);recalculate();};
    polarity.onClick=[this]{sources[selected].inverted=!sources[selected].inverted;presets.setSelectedId(0,juce::dontSendNotification);refreshSelected();recalculate();};
    addAndMakeVisible(cursor);setupSlider(cursor,0.0,1.0,0.001,0.0);
    cursor.onValueChange=[this]{recalculate();};cycles.onChange=[this]{recalculate();};
    conversionTitle.setText(juce::String::fromUTF8("PHASE ↔ DELAY ↔ FREQUENCY"),juce::dontSendNotification);
    conversionTitle.setFont(juce::FontOptions(18.0f));
    conversionTitle.setColour(juce::Label::textColourId,teal);
    conversionMode.addItem("Delay + frequency -> phase",1);
    conversionMode.addItem("Phase + frequency -> delay",2);
    conversionMode.addItem("Phase + delay -> frequency",3);
    phaseLabel.setText("Phase input (degrees)",juce::dontSendNotification);
    delayLabel.setText("Delay input (ms)",juce::dontSendNotification);
    frequencyLabel.setText("Frequency input (Hz)",juce::dontSendNotification);
    branchLabel.setText("Full cycles k (unwrapped branch)",juce::dontSendNotification);
    for(auto* slider:{&conversionPhase,&conversionDelay,&conversionFrequency,&cycleBranch}){
        addAndMakeVisible(*slider);slider->onValueChange=[this]{recalculateConversion();};
    }
    setupSlider(conversionPhase,-720.0,720.0,0.1,-90.0);setupSlider(conversionDelay,-1000.0,1000.0,0.001,2.5);
    setupSlider(conversionFrequency,0.1,20000.0,0.1,100.0);conversionFrequency.setSkewFactorFromMidPoint(1000.0);
    setupSlider(cycleBranch,-10,10,1,0);
    conversionMode.onChange=[this]{recalculateConversion();};
    conversionMode.setSelectedId(1,juce::dontSendNotification);
    setTestScenario(0);recalculateConversion();
}
void PhasePanel::rebuildSources(){
    sourceSelect.clear(juce::dontSendNotification);
    for(std::size_t index=0;index<sources.size();++index)sourceSelect.addItem("SOURCE "+juce::String(static_cast<int>(index+1)),static_cast<int>(index+1));
    sourceSelect.setSelectedId(static_cast<int>(selected+1),juce::dontSendNotification);
    count.setSelectedId(static_cast<int>(sources.size()),juce::dontSendNotification);
}
void PhasePanel::refreshSelected(){
    if(selected>=sources.size())return;
    const auto& source=sources[selected];
    const double values[]{source.frequencyHz,source.phaseDegrees,source.delayMs,source.levelDb};
    for(std::size_t index=0;index<sourceValues.size();++index)sourceValues[index].setValue(values[index],juce::dontSendNotification);
    enabled.setToggleState(source.enabled,juce::dontSendNotification);
    polarity.setButtonText(source.inverted?"POLARITY - / INVERTED":"POLARITY +");
}
double PhasePanel::displayDuration() const {
    double lowest=20000.0;
    for(const auto& source:sources)if(source.enabled)lowest=std::min(lowest,source.frequencyHz);
    return static_cast<double>(std::max(1,cycles.getSelectedId()))/lowest;
}
void PhasePanel::setTestScenario(int scenario){
    scenario=std::clamp(scenario,0,4);sources.assign(2,{});selected=0;
    if(scenario==1)sources[1].phaseDegrees=180.0;
    if(scenario==2)sources[1].phaseDegrees=90.0;
    if(scenario==3)sources[1].delayMs=2.5;
    if(scenario==4)sources[1].frequencyHz=125.0;
    linked.setToggleState(scenario!=4,juce::dontSendNotification);cursor.setValue(0,juce::dontSendNotification);
    presets.setSelectedId(scenario+1,juce::dontSendNotification);
    rebuildSources();refreshSelected();recalculate();
}
void PhasePanel::recalculate(){
    using namespace engineering::phase;
    try{
        const auto duration=displayDuration(),time=cursor.getValue()*duration;
        sum=combine(sources,time);trace=waveform(sources,0,duration,1025);valid=true;
        const auto& source=sources[selected];
        selectedDetails.setText("Selected source: period "+number(periodMs(source.frequencyHz))+" ms\n"+
            "Delay contribution: "+number(delayPhaseDegrees(source.frequencyHz,source.delayMs))+" deg\n"+
            "Total phase at t=0: "+number(effectivePhaseDegrees(source))+" deg; wrapped "+number(wrapDegrees(effectivePhaseDegrees(source)))+" deg",
            juce::dontSendNotification);
        juce::String text;
        if(sum.activeSources==0)text="All sources disabled: sum = 0; phase undefined.\nNo audio is generated.";
        else if(sum.commonFrequency){
            text="SUM peak: "+number(sum.steadyPeak,6)+"   |   RMS: "+number(sum.steadyRms,6)+" relative units\n";
            text+=sum.phaseDefined?"SUM phase at t=0: "+number(sum.referencePhaseDegrees)+" deg (modulo 360)":
                "SUM is numerically cancelled: phase undefined at a null.";
        }else text="Mixed frequencies / no single steady-state sum phase, peak or RMS is assigned.\nInstantaneous vector sum changes with time.";
        text+="\nTime cursor: "+number(time*1000.0)+" ms; instantaneous sine sum: "+number(sum.instantaneousSample,6);
        summary.setText(text,juce::dontSendNotification);
        cursorLabel.setText("Waveform window "+number(duration*1000.0)+" ms / move cursor to inspect the wheel",juce::dontSendNotification);
    }catch(const std::exception& error){valid=false;summary.setText("Invalid model: "+juce::String::fromUTF8(error.what()),juce::dontSendNotification);}
    repaint();
}
void PhasePanel::recalculateConversion(){
    using namespace engineering::phase;
    const int mode=conversionMode.getSelectedId(),branch=static_cast<int>(cycleBranch.getValue());
    conversionPhase.setEnabled(mode!=1);conversionDelay.setEnabled(mode!=2);
    conversionFrequency.setEnabled(mode!=3);cycleBranch.setEnabled(mode!=1);
    try{
        const auto phase=conversionPhase.getValue(),delay=conversionDelay.getValue(),frequency=conversionFrequency.getValue();
        juce::String text;
        if(mode==1){
            const auto total=delayPhaseDegrees(frequency,delay);
            text="Signal phase = -0.36 * f(Hz) * delay(ms) = "+number(total)+" deg\nWrapped [-180,180): "+
                number(wrapDegrees(total))+" deg; period "+number(periodMs(frequency))+" ms; delay spans "+number(delay/periodMs(frequency))+" cycles.";
        }else if(mode==2){
            const auto output=phaseToDelayMs(phase,frequency,branch);
            text="Unwrapped branch = phase + 360*k = "+number(phase+360.0*branch)+" deg\nSigned delay = "+number(output,6)+
                " ms. Wrapped phase alone has infinitely many delay solutions.";
        }else{
            const auto output=phaseDelayToFrequencyHz(phase,delay,branch);
            text="f = -(phase + 360*k) / (0.36 * delay_ms) = "+number(output,6)+" Hz\nThis chosen branch has period "+number(periodMs(output),6)+" ms. Zero delay cannot identify a frequency.";
        }
        conversionResult.setText(text,juce::dontSendNotification);
    }catch(const std::exception& error){conversionResult.setText("No valid solution for these inputs/branch: "+juce::String::fromUTF8(error.what()),juce::dontSendNotification);}
}
juce::Colour PhasePanel::sourceColour(std::size_t index) const {
    return juce::Colour::fromHSV(std::fmod(static_cast<float>(index)*0.618034f,1.0f),0.55f,0.95f,1.0f);
}
void PhasePanel::paint(juce::Graphics& g){
    g.fillAll(background);
    g.setColour(card);g.fillRoundedRectangle(sourceBounds,6.0f);
    g.setColour(teal.withAlpha(.24f));g.drawRoundedRectangle(sourceBounds,6.0f,1.0f);
    if(converterToggle.getToggleState()){
        g.setColour(card);g.fillRoundedRectangle(rightBounds,6.0f);
        g.setColour(teal.withAlpha(.24f));g.drawRoundedRectangle(rightBounds,6.0f,1.0f);
        g.setColour(juce::Colour(0xff95aabd));g.setFont(13.0f);
        g.drawFittedText("Choose the full-cycle branch k explicitly. Positive delay retards the signal; positive phase leads it.",
            rightBounds.toNearestInt().reduced(18).withY(conversionResult.getBottom()+12).withHeight(52),juce::Justification::topLeft,3);
        return;
    }
    const auto wheel=wheelBounds,wave=waveBounds;
    g.setColour(card);g.fillRoundedRectangle(wheel,6.0f);g.fillRoundedRectangle(wave,6.0f);
    g.setColour(teal.withAlpha(.24f));g.drawRoundedRectangle(wheel,6.0f,1.0f);g.drawRoundedRectangle(wave,6.0f,1.0f);
    const auto centre=wheel.getCentre();const float radius=std::max(40.0f,std::min(wheel.getWidth(),wheel.getHeight())*.34f);
    g.setColour(grid);g.drawEllipse(centre.x-radius,centre.y-radius,2*radius,2*radius,1.0f);
    g.drawLine(centre.x-radius,centre.y,centre.x+radius,centre.y,1.0f);
    g.drawLine(centre.x,centre.y-radius,centre.x,centre.y+radius,1.0f);
    g.setColour(juce::Colours::lightgrey);g.setFont(13.0f);
    g.drawText("90",static_cast<int>(centre.x-20),static_cast<int>(centre.y-radius-21),40,20,juce::Justification::centred);
    g.drawText("0",static_cast<int>(centre.x+radius+3),static_cast<int>(centre.y-10),25,20,juce::Justification::left);
    g.drawText("-90",static_cast<int>(centre.x-20),static_cast<int>(centre.y+radius+2),40,20,juce::Justification::centred);
    g.drawText("-180",static_cast<int>(centre.x-radius-37),static_cast<int>(centre.y-10),36,20,juce::Justification::right);
    if(!valid)return;
    const double scale=std::max({1.0,sum.upperPeakBound,std::abs(sum.instantaneousPhasor)});
    auto arrow=[&](std::complex<double> value,juce::Colour colour,float thickness){
        if(std::abs(value)<=scale*1.0e-12)return;
        const float x=centre.x+radius*static_cast<float>(value.real()/scale);
        const float y=centre.y-radius*static_cast<float>(value.imag()/scale);
        g.setColour(colour);g.drawArrow(juce::Line<float>(centre.x,centre.y,x,y),thickness,7.0f,6.0f);
    };
    const auto time=cursor.getValue()*displayDuration();
    for(std::size_t index=0;index<sources.size();++index)
        if(sources[index].enabled)arrow(engineering::phase::phasor(sources[index],time),sourceColour(index),index==selected?2.6f:1.5f);
    arrow(sum.instantaneousPhasor,teal,3.2f);
    auto chart=wave.reduced(12,29);chart.removeFromLeft(35);chart.removeFromRight(5);
    g.setColour(grid);
    for(int line=0;line<=4;++line){
        const auto x=chart.getX()+chart.getWidth()*static_cast<float>(line)/4.0f;
        g.drawVerticalLine(static_cast<int>(x),chart.getY(),chart.getBottom());
    }
    g.drawHorizontalLine(static_cast<int>(chart.getCentreY()),chart.getX(),chart.getRight());
    g.setColour(juce::Colours::white);g.setFont(13.0f);
    g.drawText("Source traces + teal sum",wave.reduced(12).removeFromTop(22),juce::Justification::left);
    if(trace.adequateTemporalResolution){
        const double amplitudeScale=std::max(1.0,trace.upperPeakBound);
        const auto draw=[&](const std::vector<double>& values,juce::Colour colour,float thickness){
            juce::Path path;
            for(std::size_t point=0;point<values.size();++point){
                const auto x=chart.getX()+chart.getWidth()*static_cast<float>(point)/static_cast<float>(values.size()-1);
                const auto y=chart.getCentreY()-chart.getHeight()*0.46f*static_cast<float>(values[point]/amplitudeScale);
                if(point==0)path.startNewSubPath(x,y);else path.lineTo(x,y);
            }
            g.setColour(colour);g.strokePath(path,juce::PathStrokeType(thickness));
        };
        for(std::size_t index=0;index<sources.size();++index)if(sources[index].enabled)draw(trace.sources[index],sourceColour(index),1.2f);
        draw(trace.sum,teal,2.6f);
        g.setColour(juce::Colours::white.withAlpha(0.7f));
        const auto marker=chart.getX()+chart.getWidth()*static_cast<float>(cursor.getValue());
        g.drawVerticalLine(static_cast<int>(marker),chart.getY(),chart.getBottom());
    }else{
        g.setColour(juce::Colours::orange);
        g.drawFittedText("Waveform hidden: display resolution is too low for the highest source frequency. Link frequencies or shorten the window.",chart.toNearestInt().reduced(12),juce::Justification::centred,4);
    }
    g.setColour(juce::Colour(0xffabc1ce));g.setFont(13.0f);
    const auto bound=number(std::max(1.0,trace.upperPeakBound),1);
    g.drawText("+"+bound,wave.withX(wave.getX()+2).withY(chart.getY()).withWidth(40).withHeight(20),juce::Justification::centredRight);
    g.drawText("0",wave.withX(wave.getX()+2).withY(chart.getCentreY()-10).withWidth(40).withHeight(20),juce::Justification::centredRight);
    g.drawText("-"+bound,wave.withX(wave.getX()+2).withY(chart.getBottom()-20).withWidth(40).withHeight(20),juce::Justification::centredRight);
    const auto labelY=wave.getBottom()-25;
    g.drawText("0",juce::Rectangle<float>(chart.getX()-4,labelY,35,20),juce::Justification::centredLeft);
    g.drawText(number(trace.durationSeconds*500.0,1),juce::Rectangle<float>(chart.getCentreX()-30,labelY,60,20),juce::Justification::centred);
    g.drawText(number(trace.durationSeconds*1000.0,1)+" ms",juce::Rectangle<float>(chart.getRight()-72,labelY,78,20),juce::Justification::centredRight);
    g.drawText("CCW = + phase | wheel radius "+number(scale,2)+" | sine = vertical projection",
        rightBounds.withY(wheel.getBottom()+6).withHeight(22),juce::Justification::left);
}
void PhasePanel::resized(){
    const int width=getWidth(),height=getHeight();
    title.setBounds(18,10,std::max(200,width-280),29);notice.setBounds(18,43,width-36,28);
    converterToggle.setBounds(std::max(18,width-258),10,240,32);
    const int presetWidth=std::clamp(width/4,215,285);
    presets.setBounds(18,80,presetWidth,32);
    countLabel.setBounds(presetWidth+32,80,95,32);count.setBounds(presetWidth+126,80,62,32);
    sourceLabel.setBounds(presetWidth+202,80,94,32);sourceSelect.setBounds(presetWidth+298,80,132,32);
    linked.setBounds(presetWidth+442,80,std::max(160,width-presetWidth-460),32);
    const int sourceWidth=std::clamp(width*29/100,286,330),rightX=sourceWidth+32,rightWidth=std::max(240,width-rightX-16);
    const int contentHeight=std::max(410,height-144);
    sourceBounds={16.0f,128.0f,static_cast<float>(sourceWidth),static_cast<float>(contentHeight)};
    rightBounds={static_cast<float>(rightX),128.0f,static_cast<float>(rightWidth),static_cast<float>(contentHeight)};
    for(std::size_t index=0;index<sourceValues.size();++index){
        const int y=141+static_cast<int>(index)*57;
        sourceLabels[index].setBounds(28,y,sourceWidth-24,22);sourceValues[index].setBounds(26,y+23,sourceWidth-22,29);
    }
    enabled.setBounds(28,376,sourceWidth-24,30);polarity.setBounds(28,414,sourceWidth-24,32);
    selectedDetails.setBounds(26,462,sourceWidth-22,std::max(75,contentHeight-342));
    const int chartHeight=std::clamp(contentHeight-204,205,310),wheelWidth=std::clamp(rightWidth*37/100,215,280);
    wheelBounds={static_cast<float>(rightX),128.0f,static_cast<float>(wheelWidth),static_cast<float>(chartHeight)};
    waveBounds={wheelBounds.getRight()+12,128.0f,static_cast<float>(rightWidth-wheelWidth-12),static_cast<float>(chartHeight)};
    const int summaryY=128+chartHeight+34;
    summary.setBounds(rightX,summaryY,rightWidth,82);
    cursorLabel.setBounds(rightX,summaryY+87,rightWidth,24);
    cycles.setBounds(rightX,summaryY+116,146,32);cursor.setBounds(rightX+158,summaryY+116,std::max(60,rightWidth-158),32);
    const bool converter=converterToggle.getToggleState();
    for(auto* component:std::initializer_list<juce::Component*>{&summary,&cursorLabel,&cycles,&cursor})component->setVisible(!converter);
    for(auto* component:std::initializer_list<juce::Component*>{&conversionTitle,&conversionMode,&phaseLabel,&conversionPhase,&delayLabel,&conversionDelay,
            &frequencyLabel,&conversionFrequency,&branchLabel,&cycleBranch,&conversionResult})component->setVisible(converter);
    const int innerX=rightX+18,innerWidth=rightWidth-36,fieldWidth=(innerWidth-20)/2;
    conversionTitle.setBounds(innerX,145,innerWidth,30);conversionMode.setBounds(innerX,190,innerWidth,34);
    phaseLabel.setBounds(innerX,245,fieldWidth,24);conversionPhase.setBounds(innerX,271,fieldWidth,32);
    delayLabel.setBounds(innerX+fieldWidth+20,245,fieldWidth,24);conversionDelay.setBounds(innerX+fieldWidth+20,271,fieldWidth,32);
    frequencyLabel.setBounds(innerX,327,fieldWidth,24);conversionFrequency.setBounds(innerX,353,fieldWidth,32);
    branchLabel.setBounds(innerX+fieldWidth+20,327,fieldWidth,24);cycleBranch.setBounds(innerX+fieldWidth+20,353,fieldWidth,32);
    conversionResult.setBounds(innerX,410,innerWidth,std::max(70,std::min(116,contentHeight-220)));
}
bool PhasePanel::writeValidationArtifacts(const juce::File& directory,juce::String& error){
    const auto savedSources=sources;const auto savedSelected=selected;
    const bool savedLinked=linked.getToggleState();const auto savedPreset=presets.getSelectedId();
    const auto savedCursor=cursor.getValue();const juce::Point<int> savedSize{getWidth(),getHeight()};
    bool success=true;
    juce::String report="Phase Engine native rendering: deterministic mathematical scenarios only; no audio output or measured data.\n";
    try{
        if(!directory.isDirectory() && !directory.createDirectory())throw std::runtime_error("Cannot create Phase artifact directory");
        setSize(std::max(1180,getWidth()),std::max(780,getHeight()));
        juce::PNGImageFormat png;
        for(int scenario:{1,2}){
            setTestScenario(scenario);
            const auto result=engineering::phase::combine(sources);
            const bool correct=scenario==1?(!result.phaseDefined && result.steadyPeak<1.0e-12):
                (result.phaseDefined && std::abs(result.steadyPeak-std::sqrt(2.0))<1.0e-12 && std::abs(result.referencePhaseDegrees-45.0)<1.0e-10);
            if(!correct)throw std::runtime_error("Phase scenario invariant failed");
            const auto file=directory.getChildFile(scenario==1?"phase-cancellation.png":"phase-quadrature.png");
            auto stream=file.createOutputStream();
            if(!stream || !stream->openedOk() || !stream->setPosition(0) || !stream->truncate().wasOk() ||
               !png.writeImageToStream(createComponentSnapshot(getLocalBounds()),*stream))
                throw std::runtime_error("Cannot write Phase screenshot");
            stream->flush();
            report+=file.getFileName()+" | 100 Hz + 100 Hz; phase2="+number(sources[1].phaseDegrees)+
                " deg; delay1=delay2=0 ms; level1=level2=0 dB; peak="+number(result.steadyPeak,12)+
                "; phase_defined="+juce::String(result.phaseDefined?1:0)+"; phase_deg="+
                (result.phaseDefined?number(result.referencePhaseDegrees,12):"undefined")+"\n";
        }
        if(!directory.getChildFile("phase-validation.txt").replaceWithText(report))throw std::runtime_error("Cannot save Phase validation report");
    }catch(const std::exception& exception){success=false;error=juce::String::fromUTF8(exception.what());}
    sources=savedSources;selected=savedSelected;linked.setToggleState(savedLinked,juce::dontSendNotification);
    presets.setSelectedId(savedPreset,juce::dontSendNotification);cursor.setValue(savedCursor,juce::dontSendNotification);
    rebuildSources();refreshSelected();setSize(savedSize.x,savedSize.y);recalculate();
    return success;
}
} // namespace lah::ui
