#include "SubPanel.h"
#include "../engineering/EngineeringCore.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace lah::ui {
namespace {
void horizontal(juce::Slider& s,const char* name,double min,double max,double step,double start,const char* suffix){
    s.setName(name);s.setRange(min,max,step);s.setValue(start,juce::dontSendNotification);
    s.setSliderStyle(juce::Slider::LinearHorizontal);
    s.setTextBoxStyle(juce::Slider::TextBoxRight,false,74,28);s.setTextValueSuffix(suffix);
}
const std::array<juce::Colour,4> probeColours{juce::Colour(0xff32d8c8),juce::Colour(0xff65aaff),
    juce::Colour(0xffc88aff),juce::Colour(0xffbedb75)};
}
SubPanel::SubPanel(){
    for(auto* c:{&presetSelect,&viewSelect,&sourceSelect,&probeSelect,&referenceSelect,&editorSelect})addAndMakeVisible(*c);
    presetSelect.setName("Array template");viewSelect.setName("Prediction view");sourceSelect.setName("Edit source");
    probeSelect.setName("Simulated probe");referenceSelect.setName("Geometric time reference");
    editorSelect.setName("Controls");
    editorSelect.addItem("Array setup",1);editorSelect.addItem("Edit source",2);editorSelect.addItem("Probe / time reference",3);
    editorSelect.setSelectedId(1,juce::dontSendNotification);editorSelect.onChange=[this]{resized();repaint();};
    saveA.setButtonText("SAVE A");clearA.setButtonText("CLEAR A");
    presetSelect.addItem("Broadside x4",1);presetSelect.addItem("Cardioid front/rear x2",2);
    presetSelect.addItem("End-fire x4",3);presetSelect.addItem("L/R x2",4);
    presetSelect.addItem("Inverted stack 2F+1R",5);presetSelect.addItem("Physical arc x4",6);
    presetSelect.addItem("Arc-delay / straight x4",7);
    viewSelect.addItem("TOP VIEW 2D",1);viewSelect.addItem("PROJECTED SURFACE",2);
    for(int i=1;i<=4;++i){probeSelect.addItem("PROBE "+juce::String(i),i);referenceSelect.addItem("REF = PROBE "+juce::String(i),i);}
    presetSelect.setSelectedId(1,juce::dontSendNotification);viewSelect.setSelectedId(1,juce::dontSendNotification);
    probeSelect.setSelectedId(1,juce::dontSendNotification);referenceSelect.setSelectedId(1,juce::dontSendNotification);
    for(auto* s:{&frequency,&spacing,&sourceDelay,&sourceGain,&temperature,&sourceX,&sourceY,&sourceZ,
                 &probeX,&probeY,&probeZ,&arcDepth,&focusDistance})addAndMakeVisible(*s);
    horizontal(frequency,"Frequency",20,300,1,80," Hz");horizontal(spacing,"Spacing",0.25,5,0.05,1," m");
    horizontal(sourceDelay,"Source delay",0,500,0.01,0," ms");horizontal(sourceGain,"Source gain",-60,24,0.1,0," dB");
    horizontal(temperature,"Temperature",-20,50,1,20," C");
    horizontal(sourceX,"Source X",-50,50,0.05,0," m");horizontal(sourceY,"Source Y",-50,50,0.05,0," m");
    horizontal(sourceZ,"Source Z",0,10,0.05,0.4," m");
    horizontal(probeX,"Probe X",-16,16,0.1,0," m");horizontal(probeY,"Probe Y",-11,33,0.1,12," m");
    horizontal(probeZ,"Probe Z",0,10,0.05,1.2," m");
    horizontal(arcDepth,"Physical arc depth",0,10,0.05,1.5," m");
    horizontal(focusDistance,"Arc-delay focus Y",0.5,200,0.1,12," m");
    for(auto* button:{&invert,&sourceEnabled,&referencePhase,&focusProbe,&saveA,&clearA,&resetPreset,&importData,&clearData})
        addAndMakeVisible(*button);
    for(auto* label:{&info,&caution,&definition})addAndMakeVisible(*label);
    referencePhase.setClickingTogglesState(true);referencePhase.setToggleState(true,juce::dontSendNotification);
    importData.onClick=[this]{importProfile();};clearData.onClick=[this]{profile={};recalc();};
    saveA.onClick=[this]{savedSubs=subs;savedSpeed=params.speedOfSound;savedProfile=profile;recalc();};
    clearA.onClick=[this]{savedSubs.clear();savedProfile={};savedTrace={};recalc();};
    resetPreset.onClick=[this]{rebuildPreset();};
    info.setColour(juce::Label::textColourId,juce::Colour(0xffd9f4ef));
    definition.setColour(juce::Label::textColourId,juce::Colour(0xffa5b8c9));definition.setFont(juce::Font(juce::FontOptions(13.0f)));
    caution.setColour(juce::Label::textColourId,juce::Colour(0xffffbf69));caution.setFont(juce::Font(juce::FontOptions(13.0f)));
    info.setFont(juce::FontOptions(14.0f));
    for(auto* label:{&info,&caution,&definition})label->setMinimumHorizontalScale(1.0f);
    presetSelect.onChange=[this]{rebuildPreset();};
    spacing.onValueChange=[this]{rebuildPreset();};
    arcDepth.onValueChange=[this]{if(presetSelect.getSelectedId()==6)rebuildPreset();};
    focusDistance.onValueChange=[this]{if(presetSelect.getSelectedId()==7)rebuildPreset();};
    frequency.onValueChange=[this]{recalc();};
    temperature.onValueChange=[this]{recalc();}; // existing electronic delays stay fixed until RESET
    viewSelect.onChange=[this]{repaint();};sourceSelect.onChange=[this]{loadSub();};
    auto editSource=[this]{
        const auto i=sourceSelect.getSelectedId()-1;
        if(i<0||i>=static_cast<int>(subs.size()))return;
        auto& source=subs[static_cast<std::size_t>(i)];
        source.position={sourceX.getValue(),sourceY.getValue(),sourceZ.getValue()};
        source.delayMs=sourceDelay.getValue();source.gainDb=sourceGain.getValue();
        geometryEdited=true;recalc();
    };
    for(auto* slider:{&sourceX,&sourceY,&sourceZ,&sourceDelay,&sourceGain})slider->onValueChange=editSource;
    invert.onClick=[this]{
        const auto i=sourceSelect.getSelectedId()-1;
        if(i>=0&&i<static_cast<int>(subs.size())){auto& s=subs[static_cast<std::size_t>(i)];s.invert=!s.invert;geometryEdited=true;loadSub();recalc();}
    };
    sourceEnabled.onClick=[this]{
        const auto i=sourceSelect.getSelectedId()-1;
        if(i>=0&&i<static_cast<int>(subs.size())){auto& s=subs[static_cast<std::size_t>(i)];s.enabled=!s.enabled;geometryEdited=true;loadSub();recalc();}
    };
    probeSelect.onChange=[this]{loadProbe();recalc();};
    auto editProbe=[this]{
        const auto i=probeSelect.getSelectedId()-1;
        if(i>=0&&i<4){probes[static_cast<std::size_t>(i)]={probeX.getValue(),probeY.getValue(),probeZ.getValue()};recalc();}
    };
    for(auto* slider:{&probeX,&probeY,&probeZ})slider->onValueChange=editProbe;
    referenceSelect.onChange=[this]{recalc();};
    referencePhase.onClick=[this]{referencePhase.setButtonText(referencePhase.getToggleState()?"TIME REF: ON":"TIME REF: OFF");recalc();};
    focusProbe.onClick=[this]{
        const auto i=probeSelect.getSelectedId()-1;
        if(i<0||i>=4)return;
        auto aligned=simulation::alignAtPoint(subs,probes[static_cast<std::size_t>(i)],params.speedOfSound);
        if(!aligned.empty()){subs=std::move(aligned);geometryEdited=true;loadSub();recalc();}
    };
    focusProbe.setTooltip("Add only source delays so direct paths meet at selected simulated probe; polarity is preserved. Does not write to audio hardware.");
    referencePhase.setTooltip("Remove the earliest geometric arrival at the reference probe from every phase trace. This is a model time origin, not a measured delay finder.");
    temperature.setTooltip("Changes propagation speed; existing electronic delays remain fixed. RESET PRESET recalculates template delays at this temperature.");
    loadProbe();rebuildPreset();
}
void SubPanel::rebuildPreset(){
    params.speedOfSound=engineering::speedOfSound(temperature.getValue());presetSpeed=params.speedOfSound;
    subs=simulation::preset(presetSelect.getSelectedId()-1,spacing.getValue(),presetSpeed,arcDepth.getValue(),focusDistance.getValue());
    geometryEdited=false;sourceSelect.clear(juce::dontSendNotification);
    for(std::size_t i=0;i<subs.size();++i)sourceSelect.addItem("SUB "+juce::String(static_cast<int>(i+1)),static_cast<int>(i+1));
    sourceSelect.setSelectedId(1,juce::dontSendNotification);
    arcDepth.setEnabled(presetSelect.getSelectedId()==6);focusDistance.setEnabled(presetSelect.getSelectedId()==7);
    loadSub();recalc();
}
void SubPanel::setValidationScenario(int scenario){
    presetSelect.setSelectedId(std::clamp(scenario,0,6)+1,juce::sendNotificationSync);
    recalc();
}
void SubPanel::importProfile(){
    filePicker=std::make_unique<juce::FileChooser>("Import independent polar measurements",juce::File{},"*.csv");
    auto safe=juce::Component::SafePointer<SubPanel>(this);
    filePicker->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
        [safe](const juce::FileChooser& picker){
            if(auto* self=safe.getComponent()){
                const auto file=picker.getResult();
                if(!file.existsAsFile())return;
                if(file.getSize()>10*1024*1024){
                    self->caution.setText("Rejected: polar file >10MB",juce::dontSendNotification);return;
                }
                try {
                    auto rows=juce::StringArray::fromLines(file.loadFileAsString());
                    std::vector<simulation::PolarPoint> points;
                    for(const auto& row:rows){
                        const auto line=row.trim();
                        if(line.isEmpty()||line.startsWithChar('#')||line.startsWithIgnoreCase("frequency_hz"))continue;
                        const auto values=juce::StringArray::fromTokens(line,",","");
                        if(values.size()!=4)throw std::runtime_error("Expected 4 CSV columns");
                        double x[4]{};
                        for(int k=0;k<4;++k){
                            const auto token=values[k].trim().toStdString();
                            std::size_t used=0;
                            x[k]=std::stod(token,&used);
                            if(used!=token.size())throw std::runtime_error("Invalid numeric column");
                        }
                        points.push_back({x[0],x[1],x[2],x[3]});
                        if(points.size()>100000)throw std::runtime_error("Too many points");
                    }
                    auto loaded=simulation::PolarProfile::load(points);
                    if(!loaded.valid)throw std::runtime_error(loaded.error);
                    self->profile=std::move(loaded);self->recalc();
                } catch(const std::exception& e){
                    self->profile={};self->recalc();
                    self->caution.setText("POLAR CSV REJECTED: "+juce::String(e.what()),juce::dontSendNotification);
                }
            }
        });
}

void SubPanel::loadSub(){
    const auto i=sourceSelect.getSelectedId()-1;
    if(i<0||i>=static_cast<int>(subs.size()))return;
    const auto& source=subs[static_cast<std::size_t>(i)];
    sourceX.setValue(source.position.x,juce::dontSendNotification);sourceY.setValue(source.position.y,juce::dontSendNotification);
    sourceZ.setValue(source.position.z,juce::dontSendNotification);sourceDelay.setValue(source.delayMs,juce::dontSendNotification);
    sourceGain.setValue(source.gainDb,juce::dontSendNotification);
    invert.setButtonText(source.invert?"POLARITY -":"POLARITY +");sourceEnabled.setButtonText(source.enabled?"SOURCE ON":"SOURCE OFF");
}
void SubPanel::loadProbe(){
    const auto i=std::clamp(probeSelect.getSelectedId()-1,0,3);const auto& point=probes[static_cast<std::size_t>(i)];
    probeX.setValue(point.x,juce::dontSendNotification);probeY.setValue(point.y,juce::dontSendNotification);
    probeZ.setValue(point.z,juce::dontSendNotification);
}
simulation::FrequencyPoint SubPanel::evaluate(const std::vector<simulation::Sub>& configuration,double c,
    const simulation::PolarProfile& polar,simulation::Point3 point,double hz,double referenceMs) const {
    if(!simulation::validScene(configuration,point,{c,hz}))return {};
    if(polar.valid){
        const auto response=simulation::measuredPressure(configuration,point,{c,hz},polar);
        return response.valid?simulation::responsePoint(response.h,hz,referenceMs):simulation::FrequencyPoint{};
    }
    return simulation::responsePoint(simulation::pressure(configuration,point,{c,hz}),hz,referenceMs);
}
simulation::FrequencyTrace SubPanel::makeTrace(const std::vector<simulation::Sub>& configuration,double c,
    const simulation::PolarProfile& polar,simulation::Point3 point,double referenceMs) const {
    if(!polar.valid)return simulation::frequencyResponse(configuration,point,c,20,300,160,referenceMs);
    simulation::FrequencyTrace trace;trace.referenceDelayMs=referenceMs;
    for(int i=0;i<160;++i){
        const double hz=20*std::pow(15.0,i/159.0);
        auto value=evaluate(configuration,c,polar,point,hz,referenceMs);value.hz=hz;
        trace.valid=trace.valid||value.valid;trace.points.push_back(value);
    }
    return trace;
}
void SubPanel::recalc(){
    params.frequencyHz=frequency.getValue();params.speedOfSound=engineering::speedOfSound(temperature.getValue());
    if(subs.empty())return;
    grid=profile.valid?simulation::simulateMeasured2D(subs,params,72,42,32,44,profile)
                      :simulation::simulate2D(subs,params,72,42,32,44);
    const auto selected=static_cast<std::size_t>(std::clamp(probeSelect.getSelectedId()-1,0,3));
    const auto reference=static_cast<std::size_t>(std::clamp(referenceSelect.getSelectedId()-1,0,3));
    for(std::size_t i=0;i<probes.size();++i)arrivals[i]=simulation::earliestArrival(subs,probes[i],params.speedOfSound);
    const double referenceMs=referencePhase.getToggleState()&&arrivals[reference].valid?arrivals[reference].milliseconds:0;
    for(std::size_t i=0;i<probes.size();++i){
        readings[i]=evaluate(subs,params.speedOfSound,profile,probes[i],params.frequencyHz,referenceMs);
        traces[i]=makeTrace(subs,params.speedOfSound,profile,probes[i],referenceMs);
    }
    savedTrace=savedSubs.empty()?simulation::FrequencyTrace{}:makeTrace(savedSubs,savedSpeed,savedProfile,probes[selected],referenceMs);
    const auto& reading=readings[selected];
    juce::String summary=(profile.valid?"POLAR":"OMNI")+juce::String(" | Probe ")+juce::String(static_cast<int>(selected+1));
    summary+=" @ "+juce::String(params.frequencyHz,0)+" Hz: ";
    summary+=reading.valid?juce::String(reading.relativeDb,1)+" dB relative":"outside measured profile";
    summary+=reading.phaseValid?" | phase "+juce::String(reading.phaseDeg,1)+" deg":" | phase unavailable";
    if(arrivals[selected].valid)summary+=" | arrival "+juce::String(arrivals[selected].milliseconds,2)+" ms";
    if(arrivals[selected].valid&&arrivals[reference].valid)summary+=" | vs REF "+juce::String(arrivals[selected].milliseconds-arrivals[reference].milliseconds,2)+" ms";
    summary+=savedSubs.empty()?" | A empty":" | A saved (orange)";
    info.setText(summary,juce::dontSendNotification);
    info.setTooltip(summary);
    definition.setText(juce::String(geometryEdited?"CUSTOM / template: ":"TEMPLATE: ")+simulation::presetDescription(presetSelect.getSelectedId()-1)
        +" [template c="+juce::String(presetSpeed,1)+"; current c="+juce::String(params.speedOfSound,1)+" m/s]",juce::dontSendNotification);
    definition.setTooltip(definition.getText());
    caution.setText("MODEL ONLY / Relative pressure and geometric arrivals. No calibrated SPL, coherence, room or ground response."
        +juce::String(profile.valid?" Polar slices share one time origin; no frequency extrapolation.":""),juce::dontSendNotification);
    caution.setTooltip("Predictions do not measure hardware alignment. Imported polar slices must share one profile and time origin; frequency extrapolation is disabled.");
    repaint();
}
juce::Colour SubPanel::levelColour(float db) const {
    const float t=std::clamp((db+40.0f)/55.0f,0.0f,1.0f);
    return juce::Colour::fromHSV((1.0f-t)*0.7f,0.83f,0.18f+t*0.75f,1.0f);
}
void SubPanel::paint(juce::Graphics& g){
    g.fillAll(juce::Colour(0xff101419));
    g.setColour(juce::Colour(0xff181e25));g.fillRoundedRectangle(8.0f,4.0f,static_cast<float>(getWidth()-16),115.0f,5.0f);
    g.setColour(juce::Colour(0xff22cfae).withAlpha(.23f));g.drawRoundedRectangle(8.0f,4.0f,static_cast<float>(getWidth()-16),115.0f,5.0f,1.0f);
    g.setColour(juce::Colour(0xffa5b8c9));g.setFont(13.0f);
    for(auto* c:std::initializer_list<juce::Component*>{&presetSelect,&viewSelect,&sourceSelect,&probeSelect,&referenceSelect,&editorSelect,
        &frequency,&spacing,&sourceDelay,&sourceGain,&temperature,&sourceX,&sourceY,&sourceZ,&probeX,&probeY,&probeZ,&arcDepth,&focusDistance})
        if(c->isVisible())g.drawText(c->getName(),c->getX()+2,c->getY()-21,c->getWidth()-4,20,juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xff0a1118));g.fillRoundedRectangle(mapBounds,5);
    g.setColour(juce::Colour(0xff22cfae).withAlpha(.25f));g.drawRoundedRectangle(mapBounds,5.0f,1.0f);
    const bool perspective=viewSelect.getSelectedId()==2;
    if(!grid.db.empty()){
        const float w=mapBounds.getWidth()/grid.width,h=mapBounds.getHeight()/grid.height;
        auto project=[&](float ix,float iy,float z)->juce::Point<float>{
            if(!perspective)return {mapBounds.getX()+ix*w,mapBounds.getY()+iy*h};
            const float nx=ix/grid.width-0.5f,ny=iy/grid.height;
            return {mapBounds.getCentreX()+nx*mapBounds.getWidth()*0.85f+(ny-0.5f)*mapBounds.getWidth()*0.17f,
                mapBounds.getY()+mapBounds.getHeight()*0.3f+ny*mapBounds.getHeight()*0.58f-z*mapBounds.getHeight()*0.22f};
        };
        g.saveState();g.reduceClipRegion(mapBounds.toNearestInt());
        for(int iy=0;iy<grid.height-1;++iy)for(int ix=0;ix<grid.width-1;++ix){
            const float db=grid.db[static_cast<std::size_t>(iy*grid.width+ix)];g.setColour(levelColour(db));
            if(!perspective)g.fillRect(mapBounds.getX()+ix*w,mapBounds.getY()+iy*h,w+1,h+1);
            else {
                const float z=std::clamp((db+40)/70,0.0f,1.0f);
                juce::Path path;path.startNewSubPath(project(static_cast<float>(ix),static_cast<float>(iy),z));
                path.lineTo(project(static_cast<float>(ix+1),static_cast<float>(iy),z));
                path.lineTo(project(static_cast<float>(ix+1),static_cast<float>(iy+1),z));
                path.lineTo(project(static_cast<float>(ix),static_cast<float>(iy+1),z));path.closeSubPath();g.fillPath(path);
            }
        }
        if(!perspective){
            auto xy=[&](simulation::Point3 point){return juce::Point<float>{
                mapBounds.getCentreX()+static_cast<float>(point.x/32)*mapBounds.getWidth(),
                mapBounds.getY()+static_cast<float>(0.25+point.y/44)*mapBounds.getHeight()};};
            g.setFont(13.0f);
            for(std::size_t i=0;i<subs.size();++i){
                const auto p=xy(subs[i].position);g.setColour(subs[i].enabled?juce::Colours::white:juce::Colours::grey);
                g.fillRect(p.x-4,p.y-4,8.0f,8.0f);g.drawText("S"+juce::String(static_cast<int>(i+1))+(subs[i].invert?"-":"+"),
                    static_cast<int>(p.x+6),static_cast<int>(p.y-10),36,20,juce::Justification::centredLeft);
            }
            for(std::size_t i=0;i<probes.size();++i){
                const auto p=xy(probes[i]);g.setColour(probeColours[i]);g.drawEllipse(p.x-6,p.y-6,12,12,2);
                g.drawLine(p.x-9,p.y,p.x+9,p.y,1);g.drawLine(p.x,p.y-9,p.x,p.y+9,1);
                g.drawText("P"+juce::String(static_cast<int>(i+1))+": "+(readings[i].valid?juce::String(readings[i].relativeDb,1)+" dB":"n/a"),
                    static_cast<int>(p.x+10),static_cast<int>(p.y-12),110,24,juce::Justification::centredLeft);
            }
        }
        g.restoreState();
    }
    g.setColour(juce::Colours::white);g.setFont(13.0f);
    g.drawText(perspective?"Projected model surface":"X +/-16 m | Y -11..33 m | audience +Y | simulated probes",
        mapBounds.reduced(8).removeFromTop(22),juce::Justification::centredLeft);
    if(grid.db.empty())g.drawText("No prediction at this frequency / profile range",mapBounds,juce::Justification::centred);
    g.drawText("-40 .. -10 .. +15 dB relative (fixed colour scale)",mapBounds.withY(mapBounds.getBottom()+3).withHeight(22),juce::Justification::centredLeft);
    const auto selected=static_cast<std::size_t>(std::clamp(probeSelect.getSelectedId()-1,0,3));
    auto drawChart=[&](juce::Rectangle<float> area,bool phase){
        g.setColour(juce::Colour(0xff0a1118));g.fillRoundedRectangle(area,5);
        g.setColour(juce::Colour(0xff22cfae).withAlpha(.25f));g.drawRoundedRectangle(area,5.0f,1.0f);
        g.setColour(juce::Colours::white);g.setFont(13.0f);
        g.drawText(phase?(referencePhase.getToggleState()?"Phase / deg / geometric reference":"Phase / deg / absolute model time")
                        :"Magnitude / dB relative / P1-4 + saved A",area.reduced(8).removeFromTop(22),juce::Justification::centredLeft);
        auto plot=area.reduced(8);plot.removeFromTop(28);plot.removeFromLeft(40);plot.removeFromRight(10);plot.removeFromBottom(25);
        if(plot.getWidth()<20||plot.getHeight()<15)return;
        const auto yFor=[&](double value){return plot.getY()+static_cast<float>(phase?(180-value)/360:(20-std::clamp(value,-60.0,20.0))/80)*plot.getHeight();};
        const auto xFor=[&](double hz){return plot.getX()+static_cast<float>(std::log(hz/20)/std::log(15.0))*plot.getWidth();};
        g.setFont(13.0f);
        for(double value:phase?std::initializer_list<double>{-180,0,180}:std::initializer_list<double>{-60,-20,20}){
            const float y=yFor(value);g.setColour(juce::Colour(0xff30414c));g.drawHorizontalLine(static_cast<int>(y),plot.getX(),plot.getRight());
            g.setColour(juce::Colour(0xffa7bdcd));
            g.drawText(juce::String(value,0),static_cast<int>(area.getX()+4),static_cast<int>(y-10),36,20,juce::Justification::centredRight);
        }
        for(double hz:{20.0,40.0,80.0,160.0,300.0}){
            const auto x=xFor(hz);g.setColour(juce::Colour(0xff30414c));g.drawVerticalLine(static_cast<int>(x),plot.getY(),plot.getBottom());
            g.setColour(juce::Colour(0xffa7bdcd));
            g.drawText(juce::String(hz,0),static_cast<int>(x-16),static_cast<int>(plot.getBottom()+3),34,20,juce::Justification::centred);
        }
        auto curve=[&](const simulation::FrequencyTrace& trace,juce::Colour colour,float thickness){
            juce::Path path;bool previous=false;double lastPhase=0;
            for(const auto& point:trace.points){
                if(!point.valid||(phase&&!point.phaseValid)){previous=false;continue;}
                const double value=phase?point.phaseDeg:point.relativeDb;
                if(!previous||(phase&&std::abs(value-lastPhase)>180))path.startNewSubPath(xFor(point.hz),yFor(value));
                else path.lineTo(xFor(point.hz),yFor(value));
                previous=true;lastPhase=value;
            }
            g.setColour(colour);g.strokePath(path,juce::PathStrokeType(thickness));
        };
        for(std::size_t i=0;i<traces.size();++i)curve(traces[i],probeColours[i].withAlpha(i==selected?1.0f:0.6f),i==selected?2.0f:1.0f);
        if(!savedSubs.empty())curve(savedTrace,juce::Colour(0xffffbf69),1.8f);
        g.setColour(juce::Colours::white.withAlpha(0.5f));g.drawVerticalLine(static_cast<int>(xFor(params.frequencyHz)),plot.getY(),plot.getBottom());
    };
    drawChart(magnitudeBounds,false);drawChart(phaseBounds,true);
}
void SubPanel::resized(){
    const float width=static_cast<float>(std::max(1,getWidth()-24));
    auto row=[&](int y,std::initializer_list<std::pair<juce::Component*,float>> items){
        float total=0;for(const auto& item:items)total+=item.second;
        float x=12;const float usable=width-6*static_cast<float>(items.size()-1);
        for(const auto& item:items){const float w=usable*item.second/total;item.first->setBounds(static_cast<int>(x),y+21,static_cast<int>(w),31);x+=w+6;}
    };
    row(7,{{&presetSelect,1.7f},{&editorSelect,1.45f},{&viewSelect,1.25f},{&frequency,1.1f},{&temperature,1.0f},{&saveA,0.65f},{&clearA,0.65f}});
    const int editor=editorSelect.getSelectedId();
    for(auto* c:std::initializer_list<juce::Component*>{&spacing,&arcDepth,&focusDistance,&resetPreset,&importData,&clearData})c->setVisible(editor==1);
    for(auto* c:std::initializer_list<juce::Component*>{&sourceSelect,&sourceX,&sourceY,&sourceZ,&sourceDelay,&sourceGain,&invert,&sourceEnabled})c->setVisible(editor==2);
    for(auto* c:std::initializer_list<juce::Component*>{&probeSelect,&probeX,&probeY,&probeZ,&referenceSelect,&referencePhase,&focusProbe})c->setVisible(editor==3);
    if(editor==1)row(63,{{&spacing,1.1f},{&arcDepth,1.2f},{&focusDistance,1.2f},{&resetPreset,1.1f},{&importData,1.3f},{&clearData,1.0f}});
    else if(editor==2)row(63,{{&sourceSelect,0.8f},{&sourceX,1.0f},{&sourceY,1.0f},{&sourceZ,1.0f},{&sourceDelay,1.25f},{&sourceGain,1.1f},{&invert,0.9f},{&sourceEnabled,0.9f}});
    else row(63,{{&probeSelect,0.9f},{&probeX,1.0f},{&probeY,1.0f},{&probeZ,1.0f},{&referenceSelect,1.3f},{&referencePhase,1.1f},{&focusProbe,1.2f}});
    info.setBounds(12,125,std::max(1,getWidth()-24),26);definition.setBounds(12,153,std::max(1,getWidth()-24),33);
    const float availableHeight=static_cast<float>(std::max(190,getHeight()-258));
    const float mapWidth=width*0.55f;
    mapBounds={12,198,mapWidth,availableHeight};
    const float chartX=mapBounds.getRight()+10,chartWidth=width-mapWidth-10;
    magnitudeBounds={chartX,198,chartWidth,(availableHeight-8)/2};
    phaseBounds={chartX,magnitudeBounds.getBottom()+8,chartWidth,(availableHeight-8)/2};
    caution.setBounds(12,getHeight()-36,std::max(1,getWidth()-24),32);
}
} // namespace lah::ui
