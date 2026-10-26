#include "NativePluginEditor.h"
#include "NativePresetControls.h"
#include "DigitalFxProPanel.h"
#include "DynamicsProPanel.h"
#include "../dsp/BuiltinSceneState.h"
#include "../dsp/SmartCompMacro.h"
#include <algorithm>
#include <cmath>
#include <complex>
#include <functional>

namespace lah::ui {
namespace {
const juce::Colour background{0xff0c111a},panel{0xff121d2a},edge{0xff2a3b4c};
const juce::Colour text{0xffe8f3fa},muted{0xff8da6b8};
constexpr float pi=juce::MathConstants<float>::pi;
using Values=std::array<float,6>;
double responseSampleRate(const audio::LiveAudioEngine& engine)noexcept {
    const double rate=engine.getSampleRate();return std::isfinite(rate) && rate>=8000 && rate<=192000?rate:48000;
}

juce::String displayName(int module) {
    if(module==builtin::SmartComp)return "SMART COMP";
    return juce::String::fromUTF8(builtin::metadata(module).name).toUpperCase();
}
bool isFeedback(int module) {return module==builtin::FeedbackGuard || module==builtin::AutoFeedback;}
bool isCompressor(int module) {return module==builtin::Dynamics || module==builtin::SmartComp || module==builtin::OpticalComp;}
float db(float amplitude) {return 20.0f*std::log10(std::max(0.000001f,amplitude));}

void card(juce::Graphics& g,juce::Rectangle<int> bounds,juce::Colour accent,const juce::String& label) {
    g.setColour(panel);g.fillRoundedRectangle(bounds.toFloat(),8);
    g.setColour(edge);g.drawRoundedRectangle(bounds.toFloat().reduced(.5f),8,1);
    g.setColour(accent);g.setFont(juce::FontOptions(12.0f,juce::Font::bold));
    g.drawFittedText(label,bounds.reduced(12,7).removeFromTop(19),juce::Justification::centredLeft,1,.85f);
}
juce::Rectangle<float> plotArea(juce::Rectangle<int> bounds) {
    return bounds.toFloat().reduced(12,9).withTrimmedTop(25).withTrimmedLeft(25).withTrimmedBottom(19);
}
void grid(juce::Graphics& g,juce::Rectangle<float> area) {
    g.setColour(juce::Colour(0xff0a111b));g.fillRect(area);
    g.setColour(edge.withAlpha(.7f));
    for(int n=0;n<=6;++n){
        const float x=area.getX()+area.getWidth()*n/6.0f;
        const float y=area.getY()+area.getHeight()*n/6.0f;
        g.drawVerticalLine(juce::roundToInt(x),area.getY(),area.getBottom());
        g.drawHorizontalLine(juce::roundToInt(y),area.getX(),area.getRight());
    }
}
void curve(juce::Graphics& g,juce::Rectangle<float> area,const std::function<double(double)>& function,
           double minimum,double maximum,juce::Colour colour,bool fill=false) {
    juce::Path path;
    for(int n=0;n<=320;++n){
        const double position=n/320.0;
        const double value=std::clamp(function(position),minimum,maximum);
        const float x=area.getX()+area.getWidth()*static_cast<float>(position);
        const float y=area.getBottom()-area.getHeight()*static_cast<float>((value-minimum)/(maximum-minimum));
        if(n==0)path.startNewSubPath(x,y);else path.lineTo(x,y);
    }
    if(fill){
        auto shade=path;shade.lineTo(area.getRight(),area.getBottom());shade.lineTo(area.getX(),area.getBottom());shade.closeSubPath();
        g.setGradientFill(juce::ColourGradient(colour.withAlpha(.22f),0,area.getY(),colour.withAlpha(.01f),0,area.getBottom(),false));
        g.fillPath(shade);
    }
    g.setColour(colour.withAlpha(.1f));g.strokePath(path,juce::PathStrokeType(6));
    g.setColour(colour);g.strokePath(path,juce::PathStrokeType(1.8f));
}

// Evaluate the same analytic RBJ sections used by the DSP. This is a parameter
// response model, never a measured spectrum or a promise about nonlinear stages.
std::complex<double> section(double hz,double cutoff,double q,double gain,double rate,int kind) {
    builtin::detail::Biquad filter;filter.filter(cutoff,q,gain,rate,kind);
    const auto z=std::polar(1.0,-2*builtin::pi*hz/rate),z2=z*z;
    return (filter.b0+filter.b1*z+filter.b2*z2)/(1.0+filter.a1*z+filter.a2*z2);
}
double frequencyResponse(int module,const Values& p,double hz,double rate,const builtin::FeedbackStatus& feedback) {
    std::complex<double> response{1,0};
    const auto hp=[&](double cutoff){response*=section(hz,cutoff,.70710678118655,0,rate,0);};
    const auto lp=[&](double cutoff){response*=section(hz,cutoff,.70710678118655,0,rate,1);};
    const auto bell=[&](double cutoff,double q,double gain){response*=section(hz,cutoff,q,gain,rate,2);};
    double trim=0,wetMix=1;
    switch(module){
        case builtin::VocalClean:hp(p[0]);bell(250,.9,p[1]);break;
        case builtin::VocalColor:hp(p[0]);bell(180,.7,p[1]);bell(3500,.8,p[2]);trim=p[5];wetMix=p[4]*.01;break;
        case builtin::FeedbackGuard:bell(p[0],p[2],p[1]);hp(p[3]);wetMix=p[5]*.01;break;
        case builtin::AutoFeedback:bell(p[0],p[2],p[1]);trim=p[5];break;
        case builtin::Equalizer:hp(p[0]);bell(100,.7,p[1]);bell(p[2],p[4],p[3]);bell(8000,.7,p[5]);break;
        case builtin::SmartEq:hp(p[0]);bell(3000,.8,p[2]);trim=p[5];break;
        case builtin::SubPro:hp(p[0]);lp(p[1]);lp(p[1]);bell(55,.8,p[2]);trim=p[5];break;
        case builtin::DrumLive:hp(p[0]);bell(90,.8,p[1]);bell(4500,.8,p[2]);break;
        case builtin::PcAudio:hp(p[0]);bell(100,.7,p[1]);bell(7000,.7,p[2]);break;
        case builtin::Analog:hp(p[0]);bell(150,.6,p[1]);bell(10000,.6,p[2]);trim=p[5];wetMix=p[4]*.01;break;
        case builtin::FourKLine:hp(p[0]);bell(100,.7,p[1]);bell(400,1.1,p[2]);bell(3000,1.1,p[3]);bell(10000,.7,p[4]);break;
        default:break;
    }
    response=1.0+(response-1.0)*wetMix;
    if(isFeedback(module))for(int n=0;n<feedback.notchCount && n<6;++n)
        if(feedback.notchHz[static_cast<std::size_t>(n)]>0)bell(feedback.notchHz[static_cast<std::size_t>(n)],30,feedback.depthDb[static_cast<std::size_t>(n)]);
    return 20*std::log10(std::max(1e-6,std::abs(response)))+trim;
}
void responsePlot(juce::Graphics& g,juce::Rectangle<int> bounds,int module,const Values& p,double rate,
                  const builtin::FeedbackStatus& feedback,juce::Colour accent) {
    card(g,bounds,accent,isFeedback(module)?"FILTER MODEL + CURRENT AUTO CUTS":"FILTER RESPONSE MODEL");
    const auto plot=plotArea(bounds);grid(g,plot);
    const double highest=std::min(20000.0,rate*.45);
    curve(g,plot,[&](double x){return frequencyResponse(module,p,20*std::pow(highest/20,x),rate,feedback);},-36,18,accent,true);
    g.setColour(muted);g.setFont(10.0f);
    for(int value:{-36,-18,0,18}){
        const float y=plot.getBottom()-plot.getHeight()*(value+36)/54.0f;
        g.drawText(juce::String(value),juce::roundToInt(plot.getX())-28,juce::roundToInt(y)-6,24,12,juce::Justification::centredRight);
    }
    for(double hz:{20.0,100.0,1000.0,10000.0}){
        if(hz>highest)continue;
        const auto x=plot.getX()+plot.getWidth()*static_cast<float>(std::log(hz/20)/std::log(highest/20));
        g.drawText(hz>=1000?juce::String(hz/1000,0)+"k":juce::String(hz,0),juce::roundToInt(x)-14,juce::roundToInt(plot.getBottom())+3,28,14,juce::Justification::centred);
    }
}
double compressionOutput(int module,const Values& p,double inputDb) {
    double threshold=p[0],ratio=p[1],makeup=p[5];
    if(module==builtin::VocalClean){threshold=p[3];ratio=p[4];}
    if(module==builtin::PcAudio){threshold=p[3];ratio=p[4];makeup=0;}
    double output=inputDb>threshold?threshold+(inputDb-threshold)/ratio:inputDb;
    if(module==builtin::Dynamics && inputDb<p[4])output+=inputDb-p[4];
    if(module==builtin::SmartComp){
        const double mix=p[4]*.01;
        output=20*std::log10((1-mix)*std::pow(10.,inputDb/20)+mix*std::pow(10.,output/20));
    }
    if(module==builtin::PcAudio)output=std::min(output,static_cast<double>(p[5]));
    return output+makeup;
}
void compressorPlot(juce::Graphics& g,juce::Rectangle<int> bounds,int module,const Values& p,juce::Colour accent) {
    card(g,bounds,accent,module==builtin::PcAudio?"COMPRESSOR / CLIP MODEL":"STATIC COMPRESSION MODEL");
    auto plot=plotArea(bounds);grid(g,plot);
    curve(g,plot,[](double x){return -60+60*x;},-60,6,muted.withAlpha(.4f));
    curve(g,plot,[&](double x){return compressionOutput(module,p,-60+60*x);},-60,6,accent);
    g.setColour(muted);g.setFont(10.0f);
    g.drawText("-60 dB",juce::roundToInt(plot.getX()),juce::roundToInt(plot.getBottom())+3,55,14,juce::Justification::left);
    g.drawText("0 dB IN",juce::roundToInt(plot.getRight())-55,juce::roundToInt(plot.getBottom())+3,55,14,juce::Justification::right);
}
void saturationPlot(juce::Graphics& g,juce::Rectangle<int> bounds,float driveDb,juce::Colour accent,const juce::String& label) {
    card(g,bounds,accent,label);auto plot=plotArea(bounds);grid(g,plot);
    const auto drive=std::pow(10.0,driveDb/20);
    curve(g,plot,[](double x){return -1+2*x;},-1,1,muted.withAlpha(.35f));
    curve(g,plot,[&](double x){const auto y=-1+2*x;return drive<=1?y:std::tanh(y*drive)/drive;},-1,1,accent);
}
void note(juce::Graphics& g,juce::Rectangle<int> bounds,const juce::String& value,juce::Colour colour,float font=13) {
    g.setColour(colour);g.setFont(juce::FontOptions(font));g.drawFittedText(value,bounds,juce::Justification::centred,5,.9f);
}
bool equalSnapshot(const builtin::SlotSnapshot& a,const builtin::SlotSnapshot& b) {
    return a.type==b.type && a.enabled==b.enabled && a.params==b.params &&
        a.extendedEq==b.extendedEq && a.eqInitialized==b.eqInitialized && a.eqBands==b.eqBands &&
        digitalfx::equal(a.digitalFxPro,b.digitalFxPro) && a.dynamicsPro==b.dynamicsPro;
}
}

class NativePluginEditor::EditorLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    EditorLookAndFeel(){
        setDefaultSansSerifTypefaceName("Segoe UI");
        setColour(juce::ComboBox::backgroundColourId,juce::Colour(0xff162230));
        setColour(juce::ComboBox::outlineColourId,edge);setColour(juce::ComboBox::textColourId,text);
        setColour(juce::PopupMenu::backgroundColourId,panel);setColour(juce::PopupMenu::textColourId,text);
        setColour(juce::TextButton::buttonColourId,juce::Colour(0xff203143));setColour(juce::TextButton::textColourOffId,text);
        setColour(juce::Slider::textBoxBackgroundColourId,juce::Colour(0xff09111c));
        setColour(juce::Slider::textBoxOutlineColourId,edge);setColour(juce::Slider::textBoxTextColourId,text);
        setColour(juce::Label::textColourId,text);setColour(juce::TextEditor::textColourId,text);
        setColour(juce::TextEditor::backgroundColourId,juce::Colour(0xff09111c));
    }
    juce::Font getLabelFont(juce::Label& label) override {return label.getFont();}
    void drawRotarySlider(juce::Graphics& g,int x,int y,int width,int height,float position,float start,float end,juce::Slider& slider) override {
        const auto colour=slider.findColour(juce::Slider::rotarySliderFillColourId);
        const auto bounds=juce::Rectangle<float>(static_cast<float>(x),static_cast<float>(y),static_cast<float>(width),static_cast<float>(height)).reduced(8);
        const float radius=std::max(3.0f,std::min(bounds.getWidth(),bounds.getHeight())*.5f),cx=bounds.getCentreX(),cy=bounds.getCentreY();
        const float angle=start+position*(end-start);
        g.setColour(colour.withAlpha(.05f));g.fillEllipse(cx-radius-8,cy-radius-8,2*radius+16,2*radius+16);
        for(int n=0;n<=10;++n){
            const float theta=start+(end-start)*n/10.0f;
            const float a=radius+3,b=radius+7;
            g.setColour(muted.withAlpha(.45f));g.drawLine(cx+std::sin(theta)*a,cy-std::cos(theta)*a,cx+std::sin(theta)*b,cy-std::cos(theta)*b,1);
        }
        juce::Path arc;arc.addCentredArc(cx,cy,radius,radius,0,start,end,true);
        g.setColour(edge);g.strokePath(arc,juce::PathStrokeType(3.0f));
        juce::Path active;active.addCentredArc(cx,cy,radius,radius,0,start,angle,true);
        g.setColour(colour.withAlpha(.2f));g.strokePath(active,juce::PathStrokeType(8));
        g.setColour(colour);g.strokePath(active,juce::PathStrokeType(2.5f));
        const float inner=radius*.79f;
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff475668),cx,cy-inner,juce::Colour(0xff0b1420),cx,cy+inner,false));
        g.fillEllipse(cx-inner,cy-inner,inner*2,inner*2);
        g.setColour(juce::Colour(0xff647386).withAlpha(.8f));g.drawEllipse(cx-inner,cy-inner,inner*2,inner*2,1);
        g.setColour(text);g.drawLine(cx+std::sin(angle)*inner*.25f,cy-std::cos(angle)*inner*.25f,cx+std::sin(angle)*inner*.78f,cy-std::cos(angle)*inner*.78f,2);
        g.setColour(colour);g.fillEllipse(cx-2,cy-2,4,4);
    }
};

class NativePluginEditor::ExtendedEqPanel final : public juce::Component, public juce::SettableTooltipClient {
public:
    explicit ExtendedEqPanel(NativePluginEditor& owner):editor(owner){
        setOpaque(true);setName("eight-band-eq");
        for(int band=0;band<builtin::eqBandCount;++band){
            auto& selector=bandButtons[static_cast<std::size_t>(band)];addAndMakeVisible(selector);
            selector.setName("eq-band-"+juce::String(band));selector.onClick=[this,band]{selectedBand=band;refresh();};
        }
        addAndMakeVisible(enabled);addAndMakeVisible(type);
        enabled.setName("eq-band-enable");enabled.onClick=[this]{if(!valid())return;auto& band=selected();band.enabled.store(!band.enabled.load(),std::memory_order_release);refresh();};
        for(int kind=0;kind<5;++kind)type.addItem(typeName(kind),kind+1);
        type.setName("eq-band-type");type.onChange=[this]{if(refreshing || !valid())return;selected().type.store(type.getSelectedId()-1,std::memory_order_release);refresh();};
        const char* names[]{"FREQUENCY / Hz","GAIN / dB","Q"};
        for(int p=0;p<3;++p){
            auto& slider=bandParameters[static_cast<std::size_t>(p)];auto& label=labels[static_cast<std::size_t>(p)];
            addAndMakeVisible(slider);addAndMakeVisible(label);label.setText(names[p],juce::dontSendNotification);
            label.setFont(juce::FontOptions(11.0f,juce::Font::bold));label.setColour(juce::Label::textColourId,muted);label.setJustificationType(juce::Justification::centred);label.setBorderSize(juce::BorderSize<int>(0));
            slider.setName(p==0?"eq-frequency":p==1?"eq-gain":"eq-q");slider.setSliderStyle(juce::Slider::LinearHorizontal);
            slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,86,23);slider.setColour(juce::Slider::trackColourId,editor.accentColour());
            slider.setRange(p==0?20:p==1?-24:.1,p==0?20000:p==1?24:18,p==0?1:.1);
            slider.setDoubleClickReturnValue(true,p==0?1000:p==1?0:1);
            slider.setTextValueSuffix(p==0?" Hz":p==1?" dB":"");
            if(p==0)slider.setSkewFactorFromMidPoint(1000);
            slider.onValueChange=[this,p]{
                if(refreshing || !valid())return;auto& band=selected();const auto value=static_cast<float>(bandParameters[static_cast<std::size_t>(p)].getValue());
                if(p==0)band.frequency.store(value,std::memory_order_release);else if(p==1)band.gainDb.store(value,std::memory_order_release);else band.q.store(value,std::memory_order_release);
                repaint();
            };
        }
        setTooltip("Drag numbered points for frequency/gain. HPF/LPF move horizontally. Mouse wheel changes selected-band Q; double-click a point toggles its band.");refresh();
    }
    void refresh(){
        if(!valid())return;refreshing=true;const auto value=builtin::snapshotEqBand(selected());
        enabled.setButtonText(value.enabled?"BAND ON":"BAND OFF");enabled.setColour(juce::TextButton::buttonColourId,value.enabled?juce::Colour(0xff205c5a):juce::Colour(0xff27313d));
        type.setSelectedId(value.type+1,juce::dontSendNotification);
        const double values[]{value.frequency,value.gainDb,value.q};
        bandParameters[0].setRange(20,std::min(20000.,responseSampleRate(editor.engine)*.45),1);
        for(std::size_t p=0;p<bandParameters.size();++p)if(!bandParameters[p].isMouseButtonDown() && !bandParameters[p].hasKeyboardFocus(true))bandParameters[p].setValue(values[p],juce::dontSendNotification);
        const bool gainUsed=value.type!=builtin::EqHighPass && value.type!=builtin::EqLowPass;bandParameters[1].setEnabled(gainUsed);
        for(int band=0;band<builtin::eqBandCount;++band){
            const auto state=builtin::snapshotEqBand(editor.target().eqBands[static_cast<std::size_t>(band)]);
            auto& selector=bandButtons[static_cast<std::size_t>(band)];selector.setButtonText(juce::String(band+1)+" "+typeName(state.type));
            selector.setColour(juce::TextButton::buttonColourId,band==selectedBand?juce::Colour(0xff24515d):state.enabled?juce::Colour(0xff1a333a):juce::Colour(0xff19232f));
        }
        refreshing=false;repaint();
    }
    void resized()override{
        auto area=getLocalBounds();graphCard=area.removeFromTop(std::clamp(getHeight()/2,115,215));
        plot=plotArea(graphCard);area.removeFromTop(7);
        auto selectors=area.removeFromTop(25);for(auto& selector:bandButtons){selector.setBounds(selectors.removeFromLeft(getWidth()/8).reduced(2,0));}
        area.removeFromTop(7);auto mode=area.removeFromTop(27);enabled.setBounds(mode.removeFromLeft(100));mode.removeFromLeft(8);type.setBounds(mode.removeFromLeft(std::min(180,mode.getWidth())));
        area.removeFromTop(3);const int column=getWidth()/3;
        for(int p=0;p<3;++p){auto cell=area.withX(p*column).withWidth(column).reduced(5,0);labels[static_cast<std::size_t>(p)].setBounds(cell.removeFromTop(16));bandParameters[static_cast<std::size_t>(p)].setBounds(cell);}
    }
    void paint(juce::Graphics& g)override{
        g.fillAll(background);card(g,graphCard,editor.accentColour(),"8 BAND PARAMETRIC EQ / RESPONSE MODEL");grid(g,plot);
        if(!valid())return;const double rate=responseSampleRate(editor.engine),highest=std::min(20000.,rate*.45);
        std::array<builtin::EqBandSnapshot,builtin::eqBandCount> states{};std::array<builtin::EqCoefficients,builtin::eqBandCount> filters{};
        for(std::size_t band=0;band<states.size();++band){states[band]=builtin::snapshotEqBand(editor.target().eqBands[band]);filters[band]=builtin::eqCoefficients(states[band],rate);}
        curve(g,plot,[&](double x){
            const auto hz=20*std::pow(highest/20,x),angle=-2*builtin::pi*hz/rate;const auto z=std::polar(1.,angle),z2=z*z;std::complex<double> response{1,0};
            for(const auto& filter:filters)response*=(filter.b0+filter.b1*z+filter.b2*z2)/(1.+filter.a1*z+filter.a2*z2);
            return 20*std::log10(std::max(1e-12,std::abs(response)));
        },-24,24,editor.accentColour(),true);
        g.setColour(muted);g.setFont(10.0f);
        for(int gain:{-24,0,24})g.drawText(juce::String(gain),juce::roundToInt(plot.getX())-27,juce::roundToInt(plot.getBottom()-plot.getHeight()*(gain+24)/48)-7,24,14,juce::Justification::centredRight);
        for(double hz:{20.,100.,1000.,10000.})if(hz<=highest){const auto x=plot.getX()+plot.getWidth()*static_cast<float>(std::log(hz/20)/std::log(highest/20));g.drawText(hz>=1000?juce::String(hz/1000,0)+"k":juce::String(hz,0),juce::roundToInt(x)-15,juce::roundToInt(plot.getBottom())+2,30,14,juce::Justification::centred);}
        for(int band=0;band<builtin::eqBandCount;++band){
            const auto point=handle(states[static_cast<std::size_t>(band)]);const auto colour=editor.accentColour().withRotatedHue(static_cast<float>(band)*.065f);
            g.setColour(states[static_cast<std::size_t>(band)].enabled?colour:edge);g.fillEllipse(point.x-8,point.y-8,16,16);
            g.setColour(band==selectedBand?text:muted);g.drawEllipse(point.x-8,point.y-8,16,16,band==selectedBand?2.0f:1.0f);
            g.setFont(10.0f);g.drawText(juce::String(band+1),juce::roundToInt(point.x)-8,juce::roundToInt(point.y)-8,16,16,juce::Justification::centred);
        }
    }
    void mouseDown(const juce::MouseEvent& event)override{
        dragging=false;if(!valid() || !plot.expanded(10).contains(event.position))return;
        float distance=18;
        for(int band=0;band<builtin::eqBandCount;++band){const float next=handle(builtin::snapshotEqBand(editor.target().eqBands[static_cast<std::size_t>(band)])).getDistanceFrom(event.position);if(next<distance){selectedBand=band;distance=next;dragging=true;}}
        refresh();
    }
    void mouseDrag(const juce::MouseEvent& event)override{if(dragging)applyPoint(event.position);}
    void mouseUp(const juce::MouseEvent&)override{dragging=false;}
    void mouseDoubleClick(const juce::MouseEvent& event)override{
        if(!valid() || !plot.expanded(10).contains(event.position))return;mouseDown(event);if(dragging){selected().enabled.store(!selected().enabled.load(),std::memory_order_release);refresh();}
    }
    void mouseWheelMove(const juce::MouseEvent& event,const juce::MouseWheelDetails& wheel)override{
        if(valid() && plot.contains(event.position)){bandParameters[2].setValue(bandParameters[2].getValue()+wheel.deltaY*3,juce::sendNotificationSync);}
    }
    bool validate(juce::String& error){
        selectedBand=7;refresh();
        type.setSelectedId(builtin::EqBell+1,juce::sendNotificationSync);
        const double probeFrequency=std::min(4200.,responseSampleRate(editor.engine)*.4);
        bandParameters[0].setValue(probeFrequency,juce::sendNotificationSync);bandParameters[1].setValue(7,juce::sendNotificationSync);bandParameters[2].setValue(2,juce::sendNotificationSync);
        if(std::abs(selected().frequency.load()-probeFrequency)>1 || selected().gainDb.load()!=7 || selected().q.load()!=2){error="Extended EQ numeric controls did not update the selected band.";return false;}
        const bool before=selected().enabled.load();enabled.onClick();if(selected().enabled.load()==before){error="Extended EQ band enable did not toggle.";return false;}
        applyPoint({plot.getX()+plot.getWidth()*.5f,plot.getY()+plot.getHeight()*.25f});
        if(std::abs(selected().gainDb.load()-12)>.001f || std::abs(selected().frequency.load()-std::sqrt(20*std::min(20000.,responseSampleRate(editor.engine)*.45)))>1){error="Extended EQ response drag did not update frequency/gain.";return false;}
        return true;
    }
private:
    static juce::String typeName(int kind){const char* const names[]{"Bell","Low shelf","High shelf","HPF","LPF"};return names[std::clamp(kind,0,4)];}
    bool valid()const{return editor.displayedModule==builtin::Equalizer && editor.target().type.load(std::memory_order_acquire)==builtin::Equalizer;}
    builtin::EqBandControl& selected(){return editor.target().eqBands[static_cast<std::size_t>(selectedBand)];}
    juce::Point<float> handle(const builtin::EqBandSnapshot& band)const{
        const double highest=std::min(20000.,responseSampleRate(editor.engine)*.45);
        const float x=plot.getX()+plot.getWidth()*static_cast<float>(std::log(std::clamp(static_cast<double>(band.frequency),20.,highest)/20)/std::log(highest/20));
        const float gain=band.type==builtin::EqHighPass || band.type==builtin::EqLowPass?0:band.gainDb;
        return {x,plot.getBottom()-plot.getHeight()*(gain+24)/48};
    }
    void applyPoint(juce::Point<float> point){
        if(!valid())return;const auto proportion=std::clamp((point.x-plot.getX())/std::max(1.0f,plot.getWidth()),0.0f,1.0f);
        const auto frequency=static_cast<float>(20*std::pow(std::min(20000.,responseSampleRate(editor.engine)*.45)/20,proportion));
        selected().frequency.store(frequency,std::memory_order_release);
        const int kind=selected().type.load();if(kind!=builtin::EqHighPass && kind!=builtin::EqLowPass)selected().gainDb.store(std::clamp((plot.getBottom()-point.y)/std::max(1.0f,plot.getHeight())*48-24,-24.0f,24.0f),std::memory_order_release);
        refresh();
    }
    NativePluginEditor& editor;
    std::array<juce::TextButton,builtin::eqBandCount> bandButtons;
    juce::TextButton enabled{"BAND OFF"};juce::ComboBox type;
    std::array<juce::Label,3> labels;std::array<juce::Slider,3> bandParameters;
    juce::Rectangle<int> graphCard;juce::Rectangle<float> plot;
    int selectedBand=0;bool refreshing=false,dragging=false;
};

class NativePluginEditor::SpectrumPanel final : public juce::Component {
public:
    explicit SpectrumPanel(NativePluginEditor& owner):editor(owner),data(std::make_unique<spectrum::Snapshot>()){setName("real-insert-spectrum");setInterceptsMouseClicks(false,false);}
    void refresh(){editor.engine.builtinSpectrumSnapshot(editor.spectrumHandle,*data);repaint();}
    void paint(juce::Graphics& g)override {
        const auto bounds=getLocalBounds();card(g,bounds,editor.accentColour(),"LIVE SPECTRUM  /  ACTUAL PRE + POST INSERT");
        auto area=plotArea(bounds).withTrimmedBottom(5);grid(g,area);
        if(!data->hasData || !data->signalPresent){
            const bool offline=!editor.engine.isDeviceRunning(),mute=!editor.engine.isPassThroughEnabled();
            const auto reason=!editor.spectrumHandle?"Analyzer capacity reached":offline?"NO AUDIO DEVICE":mute?"AUDIO MUTED":data->hasData?"SILENCE AT THIS INSERT":"WAITING FOR 2048 REAL SAMPLES";
            note(g,bounds.reduced(40,45),reason,muted,15);
        }else{
            const double maximum=std::min(20000.,data->sampleRate*.5),minimum=20;
            const auto draw=[&](const std::array<float,spectrum::binCount>& bins,juce::Colour colour){juce::Path path;
                // Peak-preserving pixel aggregation displays narrow real bins;
                // no generated curve or parameter model enters this view.
                const int pixels=std::max(2,static_cast<int>(area.getWidth()));
                for(int pixel=0;pixel<pixels;++pixel){const double x0=static_cast<double>(pixel)/pixels,x1=static_cast<double>(pixel+1)/pixels;
                    const double low=minimum*std::pow(maximum/minimum,x0),high=minimum*std::pow(maximum/minimum,x1);
                    const int first=std::clamp(static_cast<int>(std::floor(low*spectrum::fftSize/data->sampleRate)),1,spectrum::binCount-1);
                    const int last=std::clamp(static_cast<int>(std::ceil(high*spectrum::fftSize/data->sampleRate)),first,spectrum::binCount-1);
                    float level=-140;for(int bin=first;bin<=last;++bin)level=std::max(level,bins[static_cast<std::size_t>(bin)]);
                    const float x=area.getX()+static_cast<float>(pixel),y=area.getBottom()-area.getHeight()*(std::clamp(level,-120.f,6.f)+120)/126;
                    if(pixel==0)path.startNewSubPath(x,y);else path.lineTo(x,y);
                }
                g.setColour(colour);g.strokePath(path,juce::PathStrokeType(1.5f));
            };
            draw(data->preDb,juce::Colour(0xff93a6b8));draw(data->postDb,editor.accentColour());
        }
        g.setColour(muted);g.setFont(10.f);
        for(int level:{-120,-90,-60,-30,0}){const float y=area.getBottom()-area.getHeight()*(level+120)/126.f;g.drawText(juce::String(level),juce::roundToInt(area.getX())-29,juce::roundToInt(y)-6,25,12,juce::Justification::centredRight);}
        const double rate=data->hasData?data->sampleRate:responseSampleRate(editor.engine),maximum=std::min(20000.,rate*.5);
        for(double hz:{20.,100.,1000.,10000.}){if(hz>maximum)continue;const auto x=area.getX()+area.getWidth()*std::log(hz/20)/std::log(maximum/20);g.drawText(hz>=1000?juce::String(hz/1000,0)+"k":juce::String(hz,0),juce::roundToInt(x)-14,juce::roundToInt(area.getBottom())+2,28,13,juce::Justification::centred);}
        const auto legend="PRE (grey)  /  POST (colour)  |  dBFS  |  Hann 2048 / "+juce::String(rate/spectrum::fftSize,1)+" Hz bins";
        g.drawFittedText(legend,bounds.reduced(12,3).removeFromBottom(16),juce::Justification::centredRight,1,.8f);
    }
private:NativePluginEditor& editor;std::unique_ptr<spectrum::Snapshot> data;
};

NativePluginEditor::NativePluginEditor(audio::LiveAudioEngine& audio,int targetChannel,int targetSlot,bool output,bool diskEnabled)
    :engine(audio),skin(std::make_unique<EditorLookAndFeel>()),diskPresetsEnabled(diskEnabled) {
    setOpaque(true);setLookAndFeel(skin.get());
    for(auto* label:{&breadcrumb,&title,&description,&status}){
        addAndMakeVisible(*label);label->setBorderSize(juce::BorderSize<int>(0));label->setMinimumHorizontalScale(.9f);
        label->setColour(juce::Label::textColourId,muted);label->setFont(juce::FontOptions(12.0f));
    }
    title.setFont(juce::FontOptions(27.0f,juce::Font::bold));title.setColour(juce::Label::textColourId,text);
    description.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(moduleSelect);for(int module=0;module<=builtin::moduleCount;++module)
        moduleSelect.addItem(module==0?"Empty insert":juce::String::fromUTF8(builtin::metadata(module).name),module+1);
    moduleSelect.onChange=[this]{if(!refreshing)replaceModule();};
    moduleSelect.setTooltip("Choose the processor for this slot while audio is muted.");
    for(auto* button:{&bypassButton,&resetButton,&feedbackArmButton,&feedbackClearButton})addAndMakeVisible(*button);
    bypassButton.onClick=[this]{toggleBypass();};resetButton.onClick=[this]{resetParameters();};
    feedbackArmButton.onClick=[this]{toggleFeedbackArm();};
    feedbackClearButton.onClick=[this]{
        if(target().type.load(std::memory_order_acquire)!=displayedModule){refreshControls();return;}
        if(!isFeedback(displayedModule))return;
        target().feedbackResetGeneration.fetch_add(1,std::memory_order_acq_rel);if(presetControls)presetControls->rebindToCurrentSlot();updateState();repaint();
    };
    feedbackArmButton.setTooltip("ARM permits automatic cuts. DISARM retains existing cuts. Music can trigger detection.");
    for(int p=0;p<6;++p){
        auto& label=parameterLabels[static_cast<std::size_t>(p)];auto& slider=parameters[static_cast<std::size_t>(p)];
        addAndMakeVisible(label);addAndMakeVisible(slider);
        slider.setName("native-parameter-"+juce::String(p));
        label.setFont(juce::FontOptions(12.0f,juce::Font::bold));label.setJustificationType(juce::Justification::centred);
        label.setBorderSize(juce::BorderSize<int>(0));label.setMinimumHorizontalScale(.8f);
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setRotaryParameters(1.2f*pi,2.8f*pi,true);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,100,25);
        slider.onValueChange=[this,p]{
            if(refreshing)return;
            if(target().type.load(std::memory_order_acquire)!=displayedModule){refreshControls();return;}
            const auto& spec=builtin::metadata(displayedModule).params[static_cast<std::size_t>(p)];
            target().params[static_cast<std::size_t>(p)].store(std::clamp(static_cast<float>(parameters[static_cast<std::size_t>(p)].getValue()),spec.minimum,spec.maximum),std::memory_order_release);
            updateSmartMacroView();
            repaint();
        };
    }
    addAndMakeVisible(smartMacro);addAndMakeVisible(smartMacroState);
    smartMacro.setName("smart-macro");smartMacro.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    smartMacro.setRotaryParameters(1.2f*pi,2.8f*pi,true);smartMacro.setRange(-100,100,.1);
    smartMacro.setTextBoxStyle(juce::Slider::TextBoxBelow,false,100,25);
    smartMacro.setTextValueSuffix(" %");smartMacro.setDoubleClickReturnValue(true,0);
    smartMacro.setColour(juce::Slider::rotarySliderFillColourId,juce::Colour(0xff48dfb2));
    smartMacro.setTooltip("BOOST / BALANCED / COMP: changes threshold, ratio, attack, release, wet mix and makeup together. Advanced settings remain unchanged until this macro is moved.");
    smartMacroState.setFont(juce::FontOptions(12.0f,juce::Font::bold));smartMacroState.setJustificationType(juce::Justification::centred);
    smartMacroState.setBorderSize(juce::BorderSize<int>(0));
    smartMacro.onValueChange=[this]{
        if(refreshing)return;
        if(displayedModule!=builtin::SmartComp || target().type.load(std::memory_order_acquire)!=builtin::SmartComp){refreshControls();return;}
        if(!builtin::applySmartCompMacro(target(),static_cast<float>(smartMacro.getValue())))return;
        const auto current=values();refreshing=true;
        for(std::size_t p=0;p<parameters.size();++p)parameters[p].setValue(current[p],juce::dontSendNotification);
        refreshing=false;updateSmartMacroView();repaint();
    };
    eqPanel=std::make_unique<ExtendedEqPanel>(*this);addAndMakeVisible(*eqPanel);
    spectrumPanel=std::make_unique<SpectrumPanel>(*this);addAndMakeVisible(*spectrumPanel);spectrumPanel->setVisible(false);
    addAndMakeVisible(analyzerButton);analyzerButton.setName("real-analyzer-toggle");
    analyzerButton.setTooltip("View actual samples before/after this insert. FFT runs outside the audio callback. MODEL restores the editable processor graph.");
    analyzerButton.onClick=[this]{analyzerVisible=!analyzerVisible;updateSpectrumSubscription();refreshControls();};
    addAndMakeVisible(proModeButton);proModeButton.setName("native-pro-mode");
    proModeButton.setTooltip("Switch between the original six-parameter processor and the separate PRO processor. Stored PRO controls are preserved when returning to legacy mode.");
    proModeButton.onClick=[this]{
        if(target().type.load(std::memory_order_acquire)!=displayedModule || target().feedbackResetGeneration.load(std::memory_order_acquire)!=presetGeneration){refreshControls();return;}
        if(displayedModule==builtin::DigitalFx)target().digitalFxPro.enabled.store(!target().digitalFxPro.enabled.load(std::memory_order_acquire),std::memory_order_release);
        else if(displayedModule==builtin::Dynamics)target().dynamicsPro.enabled.store(!target().dynamicsPro.enabled.load(std::memory_order_acquire),std::memory_order_release);
        refreshControls();
    };
    addAndMakeVisible(eqModeButton);eqModeButton.setName("eq-mode");
    eqModeButton.setTooltip("First activation converts the legacy HPF/three EQ bands without changing their response. Eight-band edits are kept when returning to legacy mode.");
    eqModeButton.onClick=[this]{
        if(target().type.load(std::memory_order_acquire)!=builtin::Equalizer){refreshControls();return;}
        builtin::setExtendedEqEnabled(target(),!target().extendedEq.load(std::memory_order_acquire));refreshControls();
    };
    setSize(900,600);setTarget(targetChannel,targetSlot,output);startTimerHz(25);
}
NativePluginEditor::~NativePluginEditor(){stopTimer();engine.releaseBuiltinSpectrum(spectrumHandle);setLookAndFeel(nullptr);}
builtin::SlotControl& NativePluginEditor::target() noexcept {
    return (outputDomain?engine.outputBuiltinControls():engine.builtinControls()).slots[static_cast<std::size_t>(channel)][static_cast<std::size_t>(slotIndex)];
}
void NativePluginEditor::setTarget(int targetChannel,int targetSlot,bool output){
    engine.releaseBuiltinSpectrum(spectrumHandle);spectrumHandle={};
    presetControls.reset();presetModule=-1;
    digitalFxPanel.reset();dynamicsPanel.reset();proPanelModule=-1;
    channel=std::clamp(targetChannel,0,31);slotIndex=std::clamp(targetSlot,0,3);outputDomain=output;inputPeak=outputPeak=0;
    refreshControls();
}
void NativePluginEditor::updateSpectrumSubscription(){
    const bool wanted=analyzerVisible && isShowing();
    if(wanted && !spectrumHandle)spectrumHandle=engine.acquireBuiltinSpectrum(channel,slotIndex,outputDomain);
    if(!wanted && spectrumHandle){engine.releaseBuiltinSpectrum(spectrumHandle);spectrumHandle={};}
    analyzerButton.setButtonText(analyzerVisible?"MODEL":"ANALYZER");
    spectrumPanel->setVisible(analyzerVisible);if(analyzerVisible)spectrumPanel->refresh();
}
void NativePluginEditor::visibilityChanged(){if(spectrumPanel)updateSpectrumSubscription();}
void NativePluginEditor::parentHierarchyChanged(){if(spectrumPanel)updateSpectrumSubscription();}
bool NativePluginEditor::proModeEnabled()noexcept {
    return displayedModule==builtin::DigitalFx?target().digitalFxPro.enabled.load(std::memory_order_acquire):displayedModule==builtin::Dynamics && target().dynamicsPro.enabled.load(std::memory_order_acquire);
}
Values NativePluginEditor::values() const noexcept {
    Values result{};const auto& current=(outputDomain?engine.outputBuiltinControls():engine.builtinControls()).slots[static_cast<std::size_t>(channel)][static_cast<std::size_t>(slotIndex)];
    const auto& info=builtin::metadata(displayedModule);
    for(std::size_t p=0;p<result.size();++p){const auto value=current.params[p].load(std::memory_order_acquire);result[p]=std::isfinite(value)?std::clamp(value,info.params[p].minimum,info.params[p].maximum):info.params[p].defaultValue;}
    return result;
}
builtin::FeedbackStatus NativePluginEditor::feedbackStatus() const noexcept {
    return outputDomain?engine.outputBuiltinFeedbackStatus(channel,slotIndex):engine.builtinFeedbackStatus(channel,slotIndex);
}
juce::Colour NativePluginEditor::accentColour() const noexcept {
    switch(displayedModule){
        case builtin::VocalColor:return juce::Colour(0xffd277f4);
        case builtin::FeedbackGuard:return juce::Colour(0xffa08af7);
        case builtin::AutoFeedback:return juce::Colour(0xfff0c967);
        case builtin::SmartComp:return juce::Colour(0xff48dfb2);
        case builtin::OpticalComp:case builtin::Analog:return juce::Colour(0xffeac177);
        case builtin::Reverb96:return juce::Colour(0xffbe8bff);
        case builtin::DrumLive:return juce::Colour(0xfff18c8b);
        case builtin::FourKLine:return juce::Colour(0xff73d7df);
        default:return juce::Colour(0xff40cbe8);
    }
}
void NativePluginEditor::refreshControls(){
    refreshing=true;displayedModule=std::clamp(target().type.load(std::memory_order_acquire),0,builtin::moduleCount);
    const auto generation=target().feedbackResetGeneration.load(std::memory_order_acquire);
    if(!presetControls || presetModule!=displayedModule || presetGeneration!=generation){
        presetControls=std::make_unique<NativePresetControls>(engine,channel,slotIndex,outputDomain,diskPresetsEnabled);
        presetModule=displayedModule;presetGeneration=generation;presetControls->setName("native-preset-controls");addAndMakeVisible(*presetControls);
        presetControls->onApplied=[this]{refreshControls();};
    }else presetControls->reloadFromSlot();
    if(proPanelModule!=displayedModule || proPanelGeneration!=generation){
        digitalFxPanel.reset();dynamicsPanel.reset();proPanelModule=displayedModule;proPanelGeneration=generation;
        const auto changed=[this]{if(presetControls)presetControls->reloadFromSlot();repaint();};
        if(displayedModule==builtin::DigitalFx){digitalFxPanel=std::make_unique<DigitalFxProPanel>(engine,channel,slotIndex,outputDomain);addAndMakeVisible(*digitalFxPanel);digitalFxPanel->onChanged=changed;}
        if(displayedModule==builtin::Dynamics){dynamicsPanel=std::make_unique<DynamicsProPanel>(engine,channel,slotIndex,outputDomain);addAndMakeVisible(*dynamicsPanel);dynamicsPanel->onChanged=changed;}
    }
    const auto& info=builtin::metadata(displayedModule);const auto current=values();const auto accent=accentColour();
    breadcrumb.setText("CH "+juce::String(channel+1)+"  /  "+(outputDomain?"OUTPUT RETURN":"INPUT INSERT")+"  /  SLOT "+juce::String(slotIndex+1),juce::dontSendNotification);
    title.setText(displayName(displayedModule),juce::dontSendNotification);description.setText(juce::String::fromUTF8(info.description),juce::dontSendNotification);
    moduleSelect.setSelectedId(displayedModule+1,juce::dontSendNotification);
    for(int p=0;p<6;++p){
        const auto index=static_cast<std::size_t>(p);auto& slider=parameters[index];auto& label=parameterLabels[index];
        const bool visible=p<info.parameterCount;const bool advanced=(displayedModule==builtin::Equalizer && target().extendedEq.load(std::memory_order_acquire)) || proModeEnabled();
        slider.setVisible(visible && !advanced);label.setVisible(visible && !advanced);if(!visible)continue;
        const auto& spec=info.params[index];label.setText(juce::String::fromUTF8(spec.name).toUpperCase(),juce::dontSendNotification);
        label.setColour(juce::Label::textColourId,accent.brighter(.12f));
        slider.setRange(spec.minimum,spec.maximum,spec.step);slider.setSkewFactor(1.0);
        if(juce::String(spec.unit)=="Hz" && spec.maximum>spec.minimum*20 && spec.minimum>0)slider.setSkewFactorFromMidPoint(std::sqrt(static_cast<double>(spec.minimum)*spec.maximum));
        slider.setTextValueSuffix(juce::String(spec.unit).isEmpty()?juce::String():" "+juce::String::fromUTF8(spec.unit));
        slider.setValue(current[index],juce::dontSendNotification);slider.setDoubleClickReturnValue(true,spec.defaultValue);
        slider.setColour(juce::Slider::rotarySliderFillColourId,accent);
        slider.setTooltip(displayedModule==builtin::AutoFeedback && p==4
            ?"Tone advisory tolerance: legacy zero-crossing analysis only. Changes the tone advisory readout; does not change audio or automatic cuts. Double-click for default."
            :juce::String::fromUTF8(spec.name)+": drag to adjust, click its value to type, double-click to restore default.");
    }
    const bool eq=displayedModule==builtin::Equalizer,advanced=eq && target().extendedEq.load(std::memory_order_acquire);
    eqModeButton.setVisible(eq);eqModeButton.setButtonText(advanced?"LEGACY EQ":"8 BAND EQ");
    eqPanel->setVisible(advanced && !analyzerVisible);if(eq)eqPanel->refresh();
    const bool pro=proModeEnabled();proModeButton.setVisible(displayedModule==builtin::DigitalFx || displayedModule==builtin::Dynamics);
    proModeButton.setButtonText(pro?(displayedModule==builtin::DigitalFx?"LEGACY FX":"LEGACY DYNAMICS"):(displayedModule==builtin::DigitalFx?"PRO ECHO / REVERB":"PRO DYNAMICS"));
    if(digitalFxPanel){digitalFxPanel->setVisible(pro && !analyzerVisible);digitalFxPanel->refreshControls();}
    if(dynamicsPanel){dynamicsPanel->setVisible(pro && !analyzerVisible);dynamicsPanel->refreshControls();}
    updateSpectrumSubscription();
    if(advanced)description.setText("Eight independent bell/shelf/HPF/LPF bands. Drag points for frequency/gain; wheel adjusts Q.",juce::dontSendNotification);
    if(pro)description.setText(displayedModule==builtin::DigitalFx?"Mono Echo + Reverb. Scroll for all controls; TAP/BPM sets echo time.":"Gate / expander, soft-knee compressor and final sample-peak limiter. Scroll for all stages.",juce::dontSendNotification);
    refreshing=false;updateState();updateSmartMacroView();resized();repaint();
}
void NativePluginEditor::replaceModule(){
    if(engine.isPassThroughEnabled()){refreshControls();return;}
    builtin::configureSlot(target(),moduleSelect.getSelectedId()-1,false);if(presetControls)presetControls->rebindToCurrentSlot();refreshControls();
}
void NativePluginEditor::resetParameters(){
    if(target().type.load(std::memory_order_acquire)!=displayedModule){refreshControls();return;}
    if(target().feedbackResetGeneration.load(std::memory_order_acquire)!=presetGeneration){refreshControls();return;}
    const auto& info=builtin::metadata(displayedModule);
    for(int p=0;p<info.parameterCount;++p)target().params[static_cast<std::size_t>(p)].store(info.params[static_cast<std::size_t>(p)].defaultValue,std::memory_order_release);
    if(displayedModule==builtin::Equalizer)builtin::resetExtendedEq(target());
    if(displayedModule==builtin::DigitalFx)digitalfx::reset(target().digitalFxPro);
    if(displayedModule==builtin::Dynamics)dynamicspro::reset(target().dynamicsPro);
    if(presetControls)presetControls->reloadFromSlot();
    refreshControls();
}
void NativePluginEditor::toggleBypass(){
    if(target().type.load(std::memory_order_acquire)!=displayedModule){refreshControls();return;}
    if(displayedModule==builtin::None)return;
    const bool enabled=!target().enabled.load(std::memory_order_acquire);if(!enabled)target().feedbackArmed.store(false,std::memory_order_release);
    target().enabled.store(enabled,std::memory_order_release);updateState();repaint();
}
void NativePluginEditor::toggleFeedbackArm(){
    if(target().type.load(std::memory_order_acquire)!=displayedModule){refreshControls();return;}
    if(!isFeedback(displayedModule))return;
    auto& current=target();const bool armed=current.feedbackArmed.load(std::memory_order_acquire);
    if(armed)current.feedbackArmed.store(false,std::memory_order_release);
    else if(current.enabled.load(std::memory_order_acquire) && engine.isPassThroughEnabled() && engine.isDeviceRunning())current.feedbackArmed.store(true,std::memory_order_release);
    updateState();repaint();
}
void NativePluginEditor::updateState(){
    const bool loaded=displayedModule!=builtin::None,enabled=loaded && target().enabled.load(std::memory_order_acquire);
    moduleSelect.setEnabled(!engine.isPassThroughEnabled());resetButton.setEnabled(loaded);bypassButton.setEnabled(loaded);
    bypassButton.setButtonText(enabled?"ON":"BYPASS");bypassButton.setColour(juce::TextButton::buttonColourId,enabled?accentColour().darker(2.0f):juce::Colour(0xff27313d));
    const bool feedback=isFeedback(displayedModule);feedbackArmButton.setVisible(feedback);feedbackClearButton.setVisible(feedback);
    const bool armed=target().feedbackArmed.load(std::memory_order_acquire);
    feedbackArmButton.setButtonText(armed?"DISARM CUTS":"ARM AUTO CUT");
    feedbackArmButton.setEnabled(armed || (enabled && engine.isPassThroughEnabled() && engine.isDeviceRunning()));
    const bool available=engine.isDeviceRunning() && channel<(outputDomain?engine.getOutputChannelCount():engine.getInputChannelCount());
    status.setText(!loaded?"Choose a processor above. The new insert starts bypassed.":!available?"No active device channel  |  Controls are ready; connect the USB audio driver.":
        !engine.isPassThroughEnabled()?"AUDIO MUTED  |  Adjust parameters and enable the insert before starting audio.":
        enabled?"PROCESSING  |  Parameters update the selected insert live.":"BYPASSED  |  The selected insert passes dry audio.",juce::dontSendNotification);
    status.setColour(juce::Label::textColourId,enabled && engine.isPassThroughEnabled()?accentColour():muted);
}
void NativePluginEditor::updateSmartMacroView(){
    const bool smart=displayedModule==builtin::SmartComp;
    smartMacro.setVisible(smart && !analyzerVisible);smartMacroState.setVisible(smart && !analyzerVisible);if(!smart)return;
    const auto current=values();const auto inferred=builtin::inferSmartCompMacro(current);
    // A custom scene still gets a useful knob position from its threshold, but
    // refreshing the editor never changes any of its six stored parameters.
    const float thresholdAmount=std::clamp(-(current[0]+18.0f)/(current[0]>-18.0f?6.0f:12.0f)*100.0f,-100.0f,100.0f);
    if(!smartMacro.isMouseButtonDown() && !smartMacro.hasKeyboardFocus(true))
        smartMacro.setValue(inferred.matches?inferred.amount:thresholdAmount,juce::dontSendNotification);
    smartMacroState.setText(!inferred.matches?"CUSTOM":std::abs(inferred.amount)<.05f?"BALANCED":inferred.amount<0?"BOOST":"COMP",juce::dontSendNotification);
    smartMacroState.setColour(juce::Label::textColourId,inferred.matches?accentColour():juce::Colour(0xffeac177));
}
void NativePluginEditor::timerCallback(){
    if(analyzerVisible || spectrumHandle)updateSpectrumSubscription();
    if(target().type.load(std::memory_order_acquire)!=displayedModule){refreshControls();return;}
    if(target().feedbackResetGeneration.load(std::memory_order_acquire)!=presetGeneration){refreshControls();return;}
    if(displayedModule==builtin::Equalizer && eqPanel->isVisible()!=(target().extendedEq.load(std::memory_order_acquire) && !analyzerVisible)){refreshControls();return;}
    if((digitalFxPanel && digitalFxPanel->isVisible()!=(proModeEnabled() && !analyzerVisible)) || (dynamicsPanel && dynamicsPanel->isVisible()!=(proModeEnabled() && !analyzerVisible))){refreshControls();return;}
    const auto meter=engine.builtinMeterLevels(channel,slotIndex,outputDomain);
    inputPeak=engine.isPassThroughEnabled()?std::max(meter.inputPeak,inputPeak*.86f):0;
    outputPeak=engine.isPassThroughEnabled()?std::max(meter.outputPeak,outputPeak*.86f):0;
    const auto current=values();refreshing=true;
    for(std::size_t p=0;p<parameters.size();++p)if(!parameters[p].isMouseButtonDown() && !parameters[p].hasKeyboardFocus(true))parameters[p].setValue(current[p],juce::dontSendNotification);
    refreshing=false;updateState();updateSmartMacroView();if(displayedModule==builtin::Equalizer)eqPanel->refresh();if(digitalFxPanel)digitalFxPanel->refreshControls();if(dynamicsPanel)dynamicsPanel->refreshControls();if(analyzerVisible)spectrumPanel->refresh();repaint();
}

void NativePluginEditor::paintMeters(juce::Graphics& g){
    card(g,meterBounds,accentColour(),"SIGNAL");
    auto area=meterBounds.reduced(12,11).withTrimmedTop(42).withTrimmedBottom(20);
    const int width=std::max(8,(area.getWidth()-26)/2);
    for(int side=0;side<2;++side){
        auto bar=area.withX(area.getX()+side*(width+25)).withWidth(width);
        const float peak=side==0?inputPeak:outputPeak,level=std::clamp((db(peak)+60)/60,0.0f,1.0f);
        g.setColour(juce::Colour(0xff060c13));g.fillRoundedRectangle(bar.toFloat(),2);
        auto fill=bar.toFloat().reduced(2);fill.removeFromTop(fill.getHeight()*(1-level));
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xffe4d477),fill.getX(),static_cast<float>(bar.getY()),accentColour(),fill.getX(),static_cast<float>(bar.getBottom()),false));
        if(peak>0)g.fillRect(fill);
        g.setColour(edge);g.drawRoundedRectangle(bar.toFloat(),2,1);
        g.setFont(10.0f);g.setColour(muted);
        g.drawText(side==0?"IN":"OUT",bar.getX()-8,bar.getBottom()+3,width+16,15,juce::Justification::centred);
        g.drawText(peak>0.000001f?juce::String(db(peak),1):"-inf",bar.getX()-11,bar.getY()-18,width+22,15,juce::Justification::centred);
    }
}

void NativePluginEditor::paintModels(juce::Graphics& g){
    const auto p=values();const auto accent=accentColour();const auto feedback=feedbackStatus();
    const double rate=responseSampleRate(engine);
    if(displayedModule==builtin::Equalizer && target().extendedEq.load(std::memory_order_acquire))return;
    if(proModeEnabled())return;
    if(displayedModule==builtin::None){
        card(g,modelBounds,accent,"EMPTY INSERT");note(g,modelBounds.reduced(40),"Select any native processor above.\nEach knob controls a parameter in this channel's DSP slot.",muted,17);return;
    }
    auto left=modelBounds;auto right=left.removeFromRight(std::max(205,modelBounds.getWidth()/3));left.removeFromRight(12);
    if(displayedModule==builtin::SmartComp && !analyzerVisible){
        card(g,heroBounds,accent,"SMART MACRO");
        auto labels=heroBounds.reduced(12,0).withTrimmedTop(29).removeFromTop(17);
        const int labelWidth=labels.getWidth()/3;
        note(g,labels.removeFromLeft(labelWidth),"BOOST",muted,10);
        note(g,labels.removeFromLeft(labelWidth),"BALANCED",muted,10);
        note(g,labels,"COMP",muted,10);
        compressorPlot(g,modelBounds,builtin::SmartComp,p,accent);return;
    }
    if(isCompressor(displayedModule)){
        compressorPlot(g,left,displayedModule,p,accent);
        card(g,right,accent,displayedModule==builtin::Dynamics?"GATE / EXPANDER MODEL":"DUAL RELEASE MODEL");
        auto plot=plotArea(right);grid(g,plot);
        if(displayedModule==builtin::Dynamics){
            curve(g,plot,[&](double x){const double input=-100+100*x;return input<p[4]?2*input-p[4]:input;},-100,0,accent);
            note(g,right.reduced(12).removeFromBottom(18),"Gate "+juce::String(p[4],0)+" dBFS",muted,11);
        }else{
            curve(g,plot,[&](double x){return -8.685889638*x*2500/p[3];},-60,0,accent);
            curve(g,plot,[&](double x){return -8.685889638*x*2500/(p[3]*3);},-60,0,accent.withRotatedHue(.12f));
            note(g,right.reduced(12).removeFromBottom(18),"0 .. 2500 ms / fast + slow",muted,10);
        }
        return;
    }
    if(displayedModule==builtin::Fx || displayedModule==builtin::DigitalFx){
        const bool digital=displayedModule==builtin::DigitalFx;
        card(g,left,accent,digital?"DIGITAL ECHO REPEATS MODEL":"DAMPED ECHO REPEATS MODEL");
        auto plot=plotArea(left);grid(g,plot);
        const float mix=(digital?p[2]:p[0])*.01f,feedbackGain=digital?p[4]:p[2],time=digital?p[3]:p[1];
        const int repeats=30;
        for(int n=0;n<repeats;++n){
            const float arrival=(n+1)*time;if(arrival>2000)break;
            const float height=plot.getHeight()*mix*std::pow(feedbackGain,static_cast<float>(n));
            const float x=plot.getX()+plot.getWidth()*arrival/2000;
            g.setColour(accent.withAlpha(std::max(.12f,.9f-n*.04f)));g.fillRect(x,plot.getBottom()-height,4.0f,height);
        }
        note(g,left.reduced(12).removeFromBottom(20),"0 .. 2000 ms  |  "+juce::String(time,1)+" ms / repeat",muted,11);
        if(digital){
            card(g,right,accent,"QUANTIZER / SAMPLE HOLD");auto digitalPlot=plotArea(right);grid(g,digitalPlot);
            const double steps=std::ldexp(1.0,static_cast<int>(p[0])-1);
            curve(g,digitalPlot,[&](double x){const double sample=2*x-1;return std::round(sample*steps)/steps;},-1,1,accent);
            note(g,right.reduced(12).removeFromBottom(19),juce::String(p[0],0)+" bits / hold "+juce::String(p[1],0),muted,11);
        }else{
            saturationPlot(g,right,p[4],accent,"ECHO WRITE SATURATION");
            note(g,right.reduced(12).removeFromBottom(18),"Damping "+juce::String(p[3]/1000,1)+" kHz",muted,11);
        }
        return;
    }
    if(displayedModule==builtin::Reverb96){
        card(g,left,accent,"REVERB RT60 ENVELOPE MODEL");auto plot=plotArea(left);grid(g,plot);
        curve(g,plot,[&](double x){return -60*std::max(0.,x*5-p[3]*.001)/p[1];},-90,0,accent,true);
        note(g,left.reduced(12).removeFromBottom(19),"0 .. 5 s  |  RT60 "+juce::String(p[1],2)+" s",muted,11);
        card(g,right,accent,"ROOM / PREDELAY");
        auto room=right.toFloat().reduced(30,43);const auto centre=room.getCentre();
        for(int n=1;n<=4;++n){const float radius=static_cast<float>(n)*std::min(room.getWidth(),room.getHeight())*.11f*p[4]/1.5f;g.setColour(accent.withAlpha(.7f-n*.12f));g.drawEllipse(centre.x-radius,centre.y-radius,radius*2,radius*2,2);}
        note(g,right.reduced(12).removeFromBottom(34),juce::String(p[3],0)+" ms predelay\nRoom scale "+juce::String(p[4],2),muted,11);return;
    }
    responsePlot(g,left,displayedModule,p,rate,feedback,accent);
    switch(displayedModule){
        case builtin::VocalClean:{
            card(g,right,accent,"STEADY DE-ESS MODEL");auto plot=plotArea(right);grid(g,plot);
            curve(g,plot,[&](double x){const double level=std::pow(10.,(-60+60*x)/20);return -20*std::log10(1+p[2]*10*std::max(0.,level-.06));},-36,0,accent);
            note(g,right.reduced(12).removeFromBottom(20),"HF level -60 .. 0 dBFS",muted,10);break;
        }
        case builtin::VocalColor:saturationPlot(g,right,p[3],accent,"VOCAL COLOR / SOFT DRIVE");break;
        case builtin::Analog:saturationPlot(g,right,p[3],accent,"ANALOG / SOFT DRIVE");break;
        case builtin::PcAudio:compressorPlot(g,right,displayedModule,p,accent);break;
        case builtin::SmartEq:{
            card(g,right,accent,"ADAPTIVE BAND GAIN MODEL");auto plot=plotArea(right);grid(g,plot);
            curve(g,plot,[&](double x){return -20*std::log10(1+p[1]*std::max(0.,x-.55));},-18,0,accent);
            curve(g,plot,[&](double x){return -20*std::log10(1+p[3]*std::max(0.,x-.45));},-18,0,accent.withRotatedHue(.2f));
            note(g,right.reduced(12).removeFromBottom(19),"Relative low / high band energy",muted,10);break;
        }
        case builtin::DrumLive:{
            card(g,right,accent,"TRANSIENT EMPHASIS MODEL");auto plot=plotArea(right);grid(g,plot);
            curve(g,plot,[&](double x){return 20*std::log10(1+(builtin::gain(p[5])-1)*x);},0,9,accent,true);
            note(g,right.reduced(12).removeFromBottom(19),"Gate "+juce::String(p[3],0)+" dBFS / "+juce::String(p[4],0)+" ms",muted,10);break;
        }
        case builtin::SubPro:{
            card(g,right,accent,"SUB FILTER / POLARITY");auto body=right.reduced(16).withTrimmedTop(30);
            note(g,body.removeFromTop(body.getHeight()/2),juce::String(p[1],0)+" Hz",accent,33);
            note(g,body,"24 dB/oct low-pass\nPolarity "+juce::String(p[3]>=.5f?"INVERTED":"NORMAL"),muted,13);break;
        }
        case builtin::FeedbackGuard:case builtin::AutoFeedback:{
            card(g,right,accent,displayedModule==builtin::FeedbackGuard?"AI-FB / LIVE DETECTOR":"AUTO FB / LIVE DETECTOR");
            auto body=right.reduced(12,6).withTrimmedTop(30);
            note(g,body.removeFromTop(16),feedback.armed?"ARMED":"DISARMED",feedback.armed?juce::Colour(0xfff0c967):accent,12);
            note(g,body.removeFromTop(22),feedback.candidateHz>0?juce::String(feedback.candidateHz,1)+" Hz":"-- Hz",text,17);
            juce::String cuts="Cuts "+juce::String(feedback.notchCount)+" / 6";
            for(int n=0;n<feedback.notchCount && n<6;++n)cuts+="\n"+juce::String(feedback.notchHz[static_cast<std::size_t>(n)],0)+" Hz  "+juce::String(feedback.depthDb[static_cast<std::size_t>(n)],1)+" dB";
            if(displayedModule==builtin::FeedbackGuard)cuts+="\nModel score "+juce::String(feedback.modelScore*100,0)+"%";
            else {
                const float advisory=engine.isPassThroughEnabled() && engine.isDeviceRunning() && target().enabled.load(std::memory_order_acquire)
                    ?(outputDomain?engine.outputBuiltinFeedbackCandidateHz(channel,slotIndex):engine.builtinFeedbackCandidateHz(channel,slotIndex)):0;
                cuts+="\nTone advisory "+(std::isfinite(advisory)&&advisory>0?juce::String(advisory,1):juce::String("--"))+" Hz";
            }
            // Allocate every live cut its own row, including all six at the
            // smallest client size. This never replaces cuts with an ellipsis.
            const auto rows=juce::StringArray::fromLines(cuts);
            const int rowHeight=std::min(17,std::max(8,body.getHeight()/std::max(1,rows.size())));
            body.removeFromTop(std::max(0,(body.getHeight()-rowHeight*rows.size())/2));
            g.setColour(muted);g.setFont(juce::FontOptions(static_cast<float>(std::min(12,rowHeight))));
            for(const auto& row:rows)g.drawFittedText(row,body.removeFromTop(rowHeight),juce::Justification::centred,1,.8f);
            break;
        }
        case builtin::Equalizer:case builtin::FourKLine:{
            card(g,right,accent,displayedModule==builtin::FourKLine?"4K LINE / FOUR EQ BANDS":"PARAMETRIC EQ / BAND GAIN");
            auto plot=plotArea(right);grid(g,plot);const std::array<float,4> levels{p[1],displayedModule==builtin::FourKLine?p[2]:p[3],displayedModule==builtin::FourKLine?p[3]:p[5],displayedModule==builtin::FourKLine?p[4]:0};
            const int count=displayedModule==builtin::FourKLine?4:3;
            for(int n=0;n<count;++n){const float zero=plot.getCentreY(),height=plot.getHeight()*.45f*levels[static_cast<std::size_t>(n)]/18;g.setColour(accent.withAlpha(.8f));g.fillRect(plot.getX()+plot.getWidth()*(n+.2f)/count,std::min(zero,zero-height),plot.getWidth()*.5f/count,std::max(1.0f,std::abs(height)));}
            note(g,right.reduced(12).removeFromBottom(19),displayedModule==builtin::FourKLine?"100 / 400 / 3k / 10k Hz":"Low / Mid / High",muted,10);break;
        }
        default:card(g,right,accent,"PROCESSOR");break;
    }
}

void NativePluginEditor::paint(juce::Graphics& g){
    g.fillAll(background);const auto accent=accentColour();
    g.setGradientFill(juce::ColourGradient(accent.withAlpha(.06f),0,0,background,0,static_cast<float>(getHeight())*.6f,false));g.fillAll();
    g.setColour(accent.withAlpha(.5f));g.fillRect(20,77,getWidth()-40,1);
    if(!analyzerVisible)paintModels(g);paintMeters(g);
    g.setColour(edge);g.drawHorizontalLine(controlBounds.getY()-8,20.0f,static_cast<float>(getWidth()-20));
    g.setColour(muted);g.setFont(10.0f);
    g.drawText(analyzerVisible?"ACTUAL PRE/POST FFT / SIGNAL BARS SHOW REAL INSERT PEAKS":"MODELS SHOW PARAMETER RESPONSE / SIGNAL BARS SHOW REAL INSERT PEAKS",20,getHeight()-54,getWidth()-40,16,juce::Justification::centred);
}
void NativePluginEditor::resized(){
    auto bounds=getLocalBounds();const int width=bounds.getWidth(),height=bounds.getHeight();
    breadcrumb.setBounds(22,12,std::max(250,width-440),19);title.setBounds(22,32,std::max(250,width-440),36);
    moduleSelect.setBounds(width-408,23,210,31);bypassButton.setBounds(width-188,23,77,31);resetButton.setBounds(width-101,23,79,31);
    const bool feedback=isFeedback(displayedModule);
    const bool eq=displayedModule==builtin::Equalizer,pro=displayedModule==builtin::DigitalFx || displayedModule==builtin::Dynamics;
    description.setBounds(22,86,width-(feedback?420:(eq || pro)?320:154),39);
    analyzerButton.setBounds(width-122,89,100,29);
    eqModeButton.setBounds(width-292,89,160,29);
    proModeButton.setBounds(width-292,89,160,29);
    feedbackArmButton.setBounds(width-392,89,136,29);feedbackClearButton.setBounds(width-246,89,114,29);
    // Native title bars reduce a 540px outer window to about 502px of content.
    // Preserve the knob/value row and footer by shrinking the models first.
    if(presetControls)presetControls->setBounds(22,128,width-44,34);
    constexpr int plotTop=174;
    const int plotHeight=std::min(std::clamp(height*34/100,140,225),std::max(125,height-365));
    const int controlTop=plotTop+plotHeight+18;
    meterBounds={width-116,plotTop,94,plotHeight};modelBounds={22,plotTop,width-154,plotHeight};heroBounds={};
    if(eqPanel)eqPanel->setBounds(22,plotTop,width-154,std::max(245,height-239));
    if(digitalFxPanel)digitalFxPanel->setBounds(22,plotTop,width-154,std::max(245,height-239));
    if(dynamicsPanel)dynamicsPanel->setBounds(22,plotTop,width-154,std::max(245,height-239));
    if(spectrumPanel)spectrumPanel->setBounds(modelBounds);
    if(displayedModule==builtin::SmartComp && !analyzerVisible){
        heroBounds=modelBounds.removeFromLeft(std::min(285,modelBounds.getWidth()*43/100));modelBounds.removeFromLeft(12);
    }
    controlBounds={22,controlTop,width-44,std::max(95,height-controlTop-65)};
    const int count=6;
    for(int p=0;p<count;++p){
        auto cell=juce::Rectangle<int>(controlBounds.getX()+p*controlBounds.getWidth()/count,controlBounds.getY(),controlBounds.getWidth()/count,controlBounds.getHeight()).reduced(4,0);
        parameterLabels[static_cast<std::size_t>(p)].setBounds(cell.removeFromTop(22));
        parameters[static_cast<std::size_t>(p)].setBounds(cell);
    }
    if(displayedModule==builtin::SmartComp && !analyzerVisible){
        auto hero=heroBounds.reduced(12,0).withTrimmedTop(46).withTrimmedBottom(27);
        smartMacro.setBounds(hero);
        smartMacroState.setBounds(heroBounds.reduced(12,0).withTrimmedBottom(10).removeFromBottom(20));
    }
    status.setBounds(22,height-30,width-44,21);
}

bool NativePluginEditor::runControlValidation(juce::String& error){
    if(engine.isPassThroughEnabled()){error="Native editor validation requires muted audio.";return false;}
    const auto savedInput=builtin::snapshot(engine.builtinControls()),savedOutput=builtin::snapshot(engine.outputBuiltinControls());
    const int originalModule=displayedModule;bool success=true;
    const auto fail=[&](const juce::String& reason){if(success)error=reason;success=false;};
    if(displayedModule==builtin::None){error="Choose a processor before validating its editor.";return false;}
    const auto& info=builtin::metadata(displayedModule);
    if(presetControls){juce::String presetError;if(!presetControls->runValidation(presetError))fail(presetError);}
    // The analyzer changes only its capture subscription/view; no DSP control.
    const auto beforeView=builtin::snapshotSlot(target());analyzerButton.onClick();analyzerButton.onClick();
    if(!equalSnapshot(beforeView,builtin::snapshotSlot(target())))fail("Analyzer toggle changed insert processing settings.");
    for(int p=0;p<info.parameterCount;++p){
        const auto index=static_cast<std::size_t>(p);const auto& spec=info.params[index];
        const auto desired=spec.minimum+(spec.maximum-spec.minimum)*.73f;
        parameters[index].setValue(desired,juce::sendNotificationSync);
        if(std::abs(target().params[index].load()-static_cast<float>(parameters[index].getValue()))>1e-5f)fail("Knob "+juce::String(p)+" did not update the target atomic.");
        parameters[index].setValue(static_cast<double>(spec.maximum)+10000,juce::sendNotificationSync);
        if(std::abs(target().params[index].load()-spec.maximum)>1e-4f)fail("Knob "+juce::String(p)+" failed its maximum bound.");
        parameters[index].setValue(static_cast<double>(spec.minimum)-10000,juce::sendNotificationSync);
        if(std::abs(target().params[index].load()-spec.minimum)>1e-4f)fail("Knob "+juce::String(p)+" failed its minimum bound.");
    }
    if(displayedModule==builtin::SmartComp){
        const bool wasEnabled=target().enabled.load(std::memory_order_acquire);
        for(float amount:{-100.0f,0.0f,100.0f,42.0f}){
            smartMacro.setValue(amount,juce::sendNotificationSync);
            const auto expected=builtin::mapSmartCompMacro(amount);
            for(std::size_t p=0;p<expected.size();++p)if(std::abs(target().params[p].load()-expected[p])>.0001f)fail("Smart macro did not update all six real DSP parameters.");
            if(target().type.load()!=builtin::SmartComp || target().enabled.load()!=wasEnabled)fail("Smart macro changed the module or bypass state.");
        }
        parameters[5].setValue(3,juce::sendNotificationSync);
        const auto custom=values();
        if(smartMacroState.getText()!="CUSTOM")fail("Advanced editing did not mark the Smart macro as CUSTOM.");
        refreshControls();
        if(values()!=custom || smartMacroState.getText()!="CUSTOM")fail("Refreshing the Smart macro overwrote a custom setting.");
    }
    if(displayedModule==builtin::Equalizer){
        if(!target().extendedEq.load())eqModeButton.onClick();
        if(!target().extendedEq.load())fail("Extended EQ mode did not enable.");
        else {juce::String eqError;if(!eqPanel->validate(eqError))fail(eqError);}
        const auto configured=builtin::snapshotSlot(target());
        eqModeButton.onClick();if(target().extendedEq.load())fail("Extended EQ did not return to legacy mode.");
        eqModeButton.onClick();
        for(std::size_t band=0;band<configured.eqBands.size();++band)if(builtin::snapshotEqBand(target().eqBands[band])!=configured.eqBands[band])fail("Switching EQ mode erased advanced band settings.");
    }
    if(displayedModule==builtin::DigitalFx || displayedModule==builtin::Dynamics){
        if(!proModeEnabled())proModeButton.onClick();
        if(!proModeEnabled())fail("PRO processor mode did not enable.");
        juce::String proError;
        if(digitalFxPanel && !digitalFxPanel->runValidation(proError))fail(proError);
        if(dynamicsPanel && !dynamicsPanel->runValidation(proError))fail(proError);
        const auto configured=builtin::snapshotSlot(target());proModeButton.onClick();if(proModeEnabled())fail("PRO processor did not return to legacy mode.");proModeButton.onClick();
        const auto restored=builtin::snapshotSlot(target());
        if(configured.digitalFxPro.params!=restored.digitalFxPro.params || configured.dynamicsPro.params!=restored.dynamicsPro.params)fail("Switching PRO mode erased its stored parameters.");
    }
    const bool enabled=target().enabled.load();bypassButton.onClick();if(target().enabled.load()==enabled)fail("Bypass did not toggle the target slot.");
    resetButton.onClick();for(int p=0;p<info.parameterCount;++p)if(target().params[static_cast<std::size_t>(p)].load()!=info.params[static_cast<std::size_t>(p)].defaultValue)fail("Reset did not restore parameter defaults.");
    const auto afterInput=builtin::snapshot(engine.builtinControls()),afterOutput=builtin::snapshot(engine.outputBuiltinControls());
    for(int ch=0;ch<32;++ch)for(int slot=0;slot<4;++slot){
        const auto c=static_cast<std::size_t>(ch),s=static_cast<std::size_t>(slot);
        if(!(ch==channel && slot==slotIndex && !outputDomain) && !equalSnapshot(savedInput[c][s],afterInput[c][s]))fail("An unrelated input insert changed.");
        if(!(ch==channel && slot==slotIndex && outputDomain) && !equalSnapshot(savedOutput[c][s],afterOutput[c][s]))fail("An unrelated output insert changed.");
    }
    if(target().type.load()!=originalModule)fail("Editing parameters replaced the selected module.");
    // Restore only the tested slot; recall disarms detectors, which must not
    // disturb unrelated slots even in this muted-only validation path.
    const auto& original=(outputDomain?savedOutput:savedInput)[static_cast<std::size_t>(channel)][static_cast<std::size_t>(slotIndex)];
    builtin::recallSlot(target(),original);
    refreshControls();return success;
}
}
