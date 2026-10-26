#include "FrontFillPanel.h"
#include <algorithm>
#include <cmath>

namespace lah::ui {
namespace {
constexpr auto bg=0xff10141b,card=0xff19222c,teal=0xff22cfae;
engineering::AudiencePoint rayEnd(const engineering::FillSource& s,double heading,double length){
    const auto rad=heading*engineering::pi/180;
    return {s.x+length*std::sin(rad),s.y+length*std::cos(rad)};
}
}
FrontFillPanel::FrontFillPanel(){
    setOpaque(true);
    for(auto* component:std::initializer_list<juce::Component*>{&mode,&info,&warning,&probeInfo,&coordinates,
            &applyCoordinates,&copyCoordinates,&saveCoordinates})addAndMakeVisible(*component);
    mode.addItem("AUTO - straight front-fill",1);
    mode.addItem("MANUAL - straight front-fill",2);
    mode.addItem("MANUAL - aimed parabolic arc",3);
    mode.addItem("MANUAL - radial circular arc",4);
    mode.addItem("MANUAL - complete outward ring",5);
    mode.addItem("CUSTOM - x, y, azimuth CSV",6);
    mode.onChange=[this]{recalculate();};
    for(auto* label:{&info,&probeInfo,&warning}){
        label->setColour(juce::Label::textColourId,juce::Colour(0xffcfdae7));
        label->setFont(juce::FontOptions(13.0f));
        label->setMinimumHorizontalScale(1.0f);
    }
    warning.setColour(juce::Label::textColourId,juce::Colour(0xffffca8e));
    const char* names[]{"Audience width (m)","Audience line Y / ring offset (m)","Nominal full -6 dB beam (deg)",
        "Requested straight overlap (%)","Speaker count (manual)","Arc setback / circle radius (m)",
        "Circular arc spread (deg)","Circle rotation / azimuth (deg)"};
    const double mins[]{2,1,10,0,1,.1,5,-180},maxs[]{120,100,140,75,80,20,355,180},
        steps[]{.5,.5,1,1,1,.1,1,1},defaults[]{24,5,60,15,6,3,120,0};
    for(std::size_t index=0;index<values.size();++index){
        addAndMakeVisible(values[index]);addAndMakeVisible(captions[index]);
        captions[index].setText(names[index],juce::dontSendNotification);
        captions[index].setColour(juce::Label::textColourId,juce::Colour(0xffcfdae7));
        captions[index].setFont(juce::FontOptions(13.0f));captions[index].setTooltip(names[index]);
        captions[index].setMinimumHorizontalScale(1.0f);
        values[index].setRange(mins[index],maxs[index],steps[index]);
        values[index].setValue(defaults[index],juce::dontSendNotification);
        values[index].setSliderStyle(juce::Slider::LinearHorizontal);
        values[index].setTextBoxStyle(juce::Slider::TextBoxRight,false,74,28);
        values[index].setColour(juce::Slider::thumbColourId,juce::Colour(teal));
        values[index].onValueChange=[this]{recalculate();};
    }
    coordinates.setMultiLine(true);coordinates.setReturnKeyStartsNewLine(true);
    coordinates.setFont(juce::FontOptions(13.0f));
    coordinates.setText("x_m,y_m,heading_deg\n-4,0,-20\n0,0,0\n4,0,20",false);
    applyCoordinates.onClick=[this]{recalculate();};
    copyCoordinates.onClick=[this]{
        if(current.sources.empty())return;
        coordinates.setText(juce::String(engineering::fillSourcesCsv(current.sources)),false);
        mode.setSelectedId(6,juce::sendNotificationSync);
    };
    saveCoordinates.onClick=[this]{exportLayout();};
    mode.setSelectedId(1,juce::sendNotificationSync);
}
void FrontFillPanel::recalculate(){
    const int selected=mode.getSelectedId();
    const bool circular=selected==4 || selected==5,custom=selected==6;
    for(auto& value:values)value.setEnabled(!custom);
    values[0].setEnabled(selected!=5);values[1].setEnabled(true);values[2].setEnabled(true);
    values[3].setEnabled(selected==1);values[4].setEnabled(selected>=2 && !custom);
    values[5].setEnabled(selected==3 || circular);values[6].setEnabled(selected==4);values[7].setEnabled(circular);
    coordinates.setEnabled(custom);applyCoordinates.setEnabled(custom);
    current={};listeners.clear();analysis={};
    try{
        const auto count=static_cast<int>(values[4].getValue());
        if(circular)current.sources=engineering::circularFill(count,values[5].getValue(),selected==5?360:values[6].getValue(),values[7].getValue());
        else if(custom)current.sources=engineering::parseFillSourcesCsv(coordinates.getText().toStdString());
        else current=engineering::calculateFill(values[0].getValue(),values[1].getValue(),values[2].getValue(),
            values[3].getValue()/100,selected==1?0:count,selected==3?values[5].getValue():0);
        current.nominalWidthM=values[0].getValue();current.audienceDistanceM=values[1].getValue();current.beamWidthDeg=values[2].getValue();
        listeners=selected==5?engineering::audienceRing(values[5].getValue()+values[1].getValue()):
            engineering::audienceLine(values[0].getValue(),values[1].getValue());
        analysis=engineering::analyseCoverage(current.sources,listeners,current.beamWidthDeg);
        info.setText("Sources: "+juce::String(static_cast<int>(current.sources.size()))+
            "   |   nominal beam coverage: "+juce::String(analysis.coveredFraction*100,1)+"%"+
            "   |   overlap: "+juce::String(analysis.overlapFraction*100,1)+"%\n"+
            (selected==5?"Audience: full circle, radius = array radius + audience offset.":"Audience: horizontal target line at Y, sampled at 501 positions."),juce::dontSendNotification);
        warning.setText("GEOMETRY: nominal beam angles and paths only. Actual directivity, interference, SPL, ground and room response require acoustic data.\nAzimuth 0 = +Y; positive turns toward +X. Click plot for path/delay. CSV units: m, m, degrees.",juce::dontSendNotification);
        int covered=0;double nearest=1e9;
        for(const auto& source:current.sources){covered+=engineering::insideNominalBeam(source,probe,current.beamWidthDeg)?1:0;
            nearest=std::min(nearest,std::hypot(probe.x-source.x,probe.y-source.y));}
        probeInfo.setText("Probe ("+juce::String(probe.x,2)+", "+juce::String(probe.y,2)+") m  |  beams: "+juce::String(covered)+
            "  |  nearest path: "+juce::String(nearest,2)+" m / "+juce::String(1000*nearest/343.2,2)+" ms at 20 C (343.2 m/s)",juce::dontSendNotification);
    }catch(const std::exception& err){
        current={};listeners.clear();analysis={};
        warning.setText("Cannot calculate layout: "+juce::String::fromUTF8(err.what()),juce::dontSendNotification);
        info.setText("No valid geometry",juce::dontSendNotification);probeInfo.setText({},juce::dontSendNotification);
    }
    saveCoordinates.setEnabled(!current.sources.empty());copyCoordinates.setEnabled(!current.sources.empty());resized();repaint();
}
void FrontFillPanel::exportLayout(){
    if(current.sources.empty())return;
    const auto text=juce::String(engineering::fillSourcesCsv(current.sources));
    chooser=std::make_unique<juce::FileChooser>("Export speaker geometry CSV",juce::File{},"*.csv");
    auto safe=juce::Component::SafePointer<FrontFillPanel>(this);
    chooser->launchAsync(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|
        juce::FileBrowserComponent::warnAboutOverwriting,[safe,text](const juce::FileChooser& selected){
            if(auto* self=safe.getComponent()){
                const auto file=selected.getResult();if(file==juce::File{})return;
                self->warning.setText(file.replaceWithText(text)?"Geometry CSV saved: "+file.getFullPathName():"Failed to save geometry CSV",juce::dontSendNotification);
            }
        });
}
void FrontFillPanel::paint(juce::Graphics& g){
    g.fillAll(juce::Colour(bg));g.setColour(juce::Colour(card));g.fillRoundedRectangle(controlBounds,8.0f);
    g.setColour(juce::Colour(teal).withAlpha(.24f));g.drawRoundedRectangle(controlBounds,8.0f,1.0f);
    g.setColour(juce::Colour(0xff0b1119));g.fillRoundedRectangle(chartBounds,7);
    g.setColour(juce::Colour(teal).withAlpha(.24f));g.drawRoundedRectangle(chartBounds,7.0f,1.0f);
    if(mode.getSelectedId()==6){
        g.setColour(juce::Colour(0xff9bb5c8));g.setFont(13.0f);
        g.drawText("CUSTOM SOURCES / x_m, y_m, heading_deg",coordinates.getBounds().withY(coordinates.getY()-26).withHeight(22),juce::Justification::centredLeft);
    }
    if(current.sources.empty())return;
    const double length=std::max(3.0,values[1].getValue()*1.3);
    double loX=0,hiX=0,loY=0,hiY=0;
    const auto expand=[&](engineering::AudiencePoint p){loX=std::min(loX,p.x);hiX=std::max(hiX,p.x);loY=std::min(loY,p.y);hiY=std::max(hiY,p.y);};
    for(const auto& p:listeners)expand(p);
    for(const auto& s:current.sources){expand({s.x,s.y});expand(rayEnd(s,s.headingDeg-current.beamWidthDeg/2,length));expand(rayEnd(s,s.headingDeg+current.beamWidthDeg/2,length));}
    plotCentreX=(loX+hiX)/2;plotCentreY=(loY+hiY)/2;
    plotScale=std::min((chartBounds.getWidth()-64)/std::max(2.0,hiX-loX),(chartBounds.getHeight()-75)/std::max(2.0,hiY-loY));
    const auto project=[&](engineering::AudiencePoint p){return juce::Point<float>{chartBounds.getCentreX()+static_cast<float>((p.x-plotCentreX)*plotScale),chartBounds.getCentreY()-static_cast<float>((p.y-plotCentreY)*plotScale)};};
    juce::Graphics::ScopedSaveState saved(g);g.reduceClipRegion(chartBounds.toNearestInt().reduced(1));
    const double span=std::max(hiX-loX,hiY-loY),grid=std::pow(10.0,std::floor(std::log10(std::max(span/8,.1))));
    g.setColour(juce::Colour(0xff24303c));
    for(double x=std::floor(loX/grid)*grid;x<=hiX+grid;x+=grid){const auto p=project({x,0});g.drawVerticalLine(static_cast<int>(p.x),chartBounds.getY()+24,chartBounds.getBottom());}
    for(double y=std::floor(loY/grid)*grid;y<=hiY+grid;y+=grid){const auto p=project({0,y});g.drawHorizontalLine(static_cast<int>(p.y),chartBounds.getX(),chartBounds.getRight());}
    for(std::size_t index=0;index<current.sources.size();++index){const auto& s=current.sources[index];
        const auto p=project({s.x,s.y}),l=project(rayEnd(s,s.headingDeg-current.beamWidthDeg/2,length)),r=project(rayEnd(s,s.headingDeg+current.beamWidthDeg/2,length));
        juce::Path cone;cone.startNewSubPath(p);cone.lineTo(l);cone.lineTo(r);cone.closeSubPath();
        g.setColour(juce::Colour(teal).withAlpha(.055f));g.fillPath(cone);
        g.setColour(juce::Colour(teal).withAlpha(.5f));g.strokePath(cone,juce::PathStrokeType(.8f));
        g.setColour(juce::Colours::white);g.fillEllipse(p.x-3,p.y-3,6,6);
        const auto aim=project(rayEnd(s,s.headingDeg,std::min(1.0,length/4)));g.drawArrow(juce::Line<float>(p,aim),1,4,4);
        g.drawText(juce::String(static_cast<int>(index+1)),static_cast<int>(p.x-14),static_cast<int>(p.y-22),28,18,juce::Justification::centred);
    }
    for(std::size_t index=1;index<listeners.size();++index){const int count=analysis.counts[index];
        g.setColour(count==0?juce::Colour(0xffff657b):count==1?juce::Colour(teal):juce::Colour(0xffac95ff));
        g.drawLine(juce::Line<float>(project(listeners[index-1]),project(listeners[index])),3);
    }
    if(mode.getSelectedId()==5 && !listeners.empty()){g.setColour(juce::Colour(teal));g.drawLine(juce::Line<float>(project(listeners.back()),project(listeners.front())),2);}
    const auto p=project(probe);g.setColour(juce::Colours::white);g.drawEllipse(p.x-5,p.y-5,10,10,1.5f);
    g.drawLine(p.x-9,p.y,p.x+9,p.y);g.drawLine(p.x,p.y-9,p.x,p.y+9);
    g.setColour(juce::Colour(0xffcfdae7));g.setFont(juce::FontOptions(13.0f));
    g.drawText("NOMINAL BEAM GEOMETRY  |  +Y up  |  grid "+juce::String(grid,2)+" m",chartBounds.toNearestInt().reduced(10).withHeight(20),juce::Justification::left);
}
void FrontFillPanel::mouseDown(const juce::MouseEvent& event){
    if(chartBounds.contains(event.position) && !current.sources.empty()){
        probe={plotCentreX+(event.position.x-chartBounds.getCentreX())/plotScale,
               plotCentreY-(event.position.y-chartBounds.getCentreY())/plotScale};recalculate();
    }
}
void FrontFillPanel::setValidationScenario(int scenario){mode.setSelectedId(std::clamp(scenario+1,1,6),juce::sendNotificationSync);}
void FrontFillPanel::resized(){
    const int width=getWidth(),height=getHeight(),controls=std::clamp(width*30/100,305,370),plotX=controls+28;
    const bool custom=mode.getSelectedId()==6;
    mode.setBounds(16,12,std::max(200,controls),34);
    copyCoordinates.setBounds(plotX,12,180,34);saveCoordinates.setBounds(plotX+190,12,190,34);
    controlBounds={16.0f,60.0f,static_cast<float>(controls),static_cast<float>(std::max(450,height-134))};
    const int rowHeight=std::clamp((height-165)/8,50,57);
    int row=0;
    for(std::size_t index=0;index<values.size();++index){
        const bool show=!custom||index==1||index==2;
        captions[index].setVisible(show);values[index].setVisible(show);
        if(!show)continue;
        const int y=72+row++*rowHeight;
        captions[index].setBounds(26,y,controls-20,21);
        values[index].setBounds(24,y+21,controls-18,29);
    }
    coordinates.setVisible(custom);applyCoordinates.setVisible(custom);
    coordinates.setBounds(26,236,controls-20,std::max(150,height-372));
    applyCoordinates.setBounds(26,height-123,controls-20,33);
    chartBounds={static_cast<float>(plotX),60.0f,static_cast<float>(std::max(200,width-plotX-16)),static_cast<float>(std::max(280,height-230))};
    info.setBounds(plotX,height-159,std::max(200,width-plotX-16),46);
    probeInfo.setBounds(plotX,height-107,std::max(200,width-plotX-16),38);
    warning.setBounds(18,height-61,std::max(250,width-36),53);
}
} // namespace lah::ui
