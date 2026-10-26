// Audible-effect acceptance for every native module at CH8 / INSERT3.
// Uses the real DSP rack without JUCE, a device, or any UI-generated data.
#include "../src/dsp/BuiltinRackCore.h"
#include <array>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>

#define REQUIRE(condition) do { if(!(condition)) { \
    std::cerr<<"FAILED line "<<__LINE__<<": " #condition "\n";std::exit(1); } } while(false)

namespace {
using namespace lah::builtin;
constexpr int targetChannel=7,targetSlot=2,blockSize=128,period=3072,captureSize=period*2;
constexpr double sampleRate=48000;
using Recording=std::array<float,captureSize>;
enum class Signal {Tone,Multitone,Low,Noise,Impulse,Bursts};
struct Scenario {int module,parameter;float before,after;Signal signal;};
constexpr std::array<Scenario,17> scenarios{{
    {VocalClean,2,0,10,Signal::Multitone},
    {VocalColor,3,0,15,Signal::Multitone},
    {FeedbackGuard,1,0,-18,Signal::Tone},
    {AutoFeedback,1,0,-18,Signal::Tone},
    {Equalizer,3,0,-12,Signal::Multitone},
    {SmartEq,1,0,12,Signal::Low},
    {Dynamics,0,0,-30,Signal::Multitone},
    {SmartComp,0,0,-30,Signal::Multitone},
    {OpticalComp,0,0,-30,Signal::Multitone},
    {Fx,0,0,70,Signal::Impulse},
    {DigitalFx,0,24,4,Signal::Noise},
    {Reverb96,0,0,60,Signal::Impulse},
    {SubPro,1,50,180,Signal::Low},
    {DrumLive,5,0,9,Signal::Bursts},
    {PcAudio,3,0,-24,Signal::Multitone},
    {Analog,3,0,15,Signal::Multitone},
    {FourKLine,3,0,-12,Signal::Multitone}
}};

std::array<float,period> makeSignal(Signal kind){
    std::array<float,period> result{};
    std::uint32_t random=0x8167abcd;
    for(int n=0;n<period;++n){
        const auto sine=[n](double cycles){return std::sin(2*pi*cycles*n/period);};
        random^=random<<13;random^=random>>17;random^=random<<5;
        const double noise=static_cast<double>(random)/4294967295.0*2-1;
        double sample=0;
        switch(kind){
            case Signal::Tone:sample=.25*sine(64);break; // Exactly 1000 Hz.
            case Signal::Multitone:sample=.18*sine(64)+.11*sine(6)+.12*sine(192)+.14*sine(512)+.025*noise;break;
            case Signal::Low:sample=.3*sine(6)+.015*sine(96)+.01*noise;break;
            case Signal::Noise:sample=.35*noise+.1*sine(64);break;
            case Signal::Impulse:sample=n==0?.5:n==100?.2:0;break;
            case Signal::Bursts:{
                const int position=n%768;
                sample=.5*std::exp(-position/65.0)*std::sin(2*pi*80*position/period);
                break;
            }
        }
        result[static_cast<std::size_t>(n)]=static_cast<float>(sample);
    }
    return result;
}

void configureBaseline(SlotControl& slot,const Scenario& scenario){
    configureSlot(slot,scenario.module,true);
    // Remove unrelated nonlinearities where they could mask the parameter under
    // test. The changed control remains a specific processor function, never a
    // generic output-trim substitute for the module's actual effect.
    switch(scenario.module){
        case VocalClean:slot.params[3].store(0);slot.params[4].store(1);break;
        case VocalColor:slot.params[1].store(0);slot.params[2].store(0);break;
        case FeedbackGuard:slot.params[3].store(20);slot.params[4].store(0);break;
        case SmartEq:slot.params[0].store(20);slot.params[2].store(0);slot.params[3].store(0);break;
        case Dynamics:slot.params[1].store(4);slot.params[4].store(-100);break;
        case SmartComp:slot.params[1].store(4);slot.params[4].store(100);break;
        case OpticalComp:slot.params[1].store(4);slot.params[4].store(0);break;
        case Fx:slot.params[1].store(83);slot.params[4].store(0);break;
        case DigitalFx:slot.params[1].store(1);slot.params[2].store(100);slot.params[4].store(0);break;
        case Reverb96:slot.params[1].store(.5f);break;
        case SubPro:slot.params[0].store(15);slot.params[2].store(0);slot.params[5].store(0);break;
        case DrumLive:slot.params[3].store(-90);break;
        case PcAudio:slot.params[4].store(4);break;
        case Analog:slot.params[1].store(0);slot.params[2].store(0);break;
        default:break;
    }
    slot.params[static_cast<std::size_t>(scenario.parameter)].store(scenario.before);
}

void render(Rack& rack,Controls& controls,const std::array<float,period>& source,int blockCount,
            std::uint64_t& frames,Recording* recording=nullptr){
    int recorded=0;
    for(int block=0;block<blockCount;++block){
        rack.beginBlock(controls);
        for(int sample=0;sample<blockSize;++sample,++frames){
            const float dry=source[static_cast<std::size_t>(frames%period)];
            const float processed=rack.processSample(targetChannel,dry);
            REQUIRE(std::isfinite(processed)&&std::abs(processed)<=16);
            // Neighbouring and hard-coded-zero channels stay exact dry, while
            // another active insert preserves its own analytic -6 dB trim.
            REQUIRE(rack.processSample(6,dry)==dry);
            REQUIRE(rack.processSample(0,dry)==dry);
            REQUIRE(rack.processSample(5,dry)==dry*gain(-6));
            if(recording){
                REQUIRE(recorded<captureSize);
                (*recording)[static_cast<std::size_t>(recorded++)]=processed;
            }
        }
    }
    if(recording)REQUIRE(recorded==captureSize);
}

double rms(const Recording& recording){
    double energy=0;
    for(float sample:recording)energy+=static_cast<double>(sample)*sample;
    return std::sqrt(energy/captureSize);
}
double difference(const Recording& a,const Recording& b){
    double energy=0;
    for(std::size_t n=0;n<a.size();++n){const double delta=static_cast<double>(a[n])-b[n];energy+=delta*delta;}
    return std::sqrt(energy/captureSize);
}
}

int main(){
    using namespace lah::builtin;
    static_assert(moduleCount==scenarios.size(),"Add an audible parameter scenario for each new native module");
    auto rack=std::make_unique<Rack>();
    Controls controls;
    configureSlot(controls.slots[5][1],VocalColor,true);
    controls.slots[5][1].params[4].store(0);
    controls.slots[5][1].params[5].store(-6);
    rack->prepare(sampleRate,blockSize);
    std::cout<<"CH8 / INSERT3: actual native DSP, parameter edits while enabled\n"
             <<"Module | Parameter | Dry RMS | Before RMS | After RMS | Before-after diff | After-dry diff | Bypass-dry diff\n";
    std::cout<<std::fixed<<std::setprecision(6);
    std::array<bool,moduleCount+1> covered{};
    for(const auto& scenario:scenarios){
        REQUIRE(scenario.module>=1&&scenario.module<=moduleCount);
        REQUIRE(!covered[static_cast<std::size_t>(scenario.module)]);
        covered[static_cast<std::size_t>(scenario.module)]=true;
        auto& slot=controls.slots[targetChannel][targetSlot];
        configureBaseline(slot,scenario);
        rack->reset(); // Initial setup only. No reset occurs for the live edit.
        const auto source=makeSignal(scenario.signal);
        Recording dry{},before{},after{},bypassed{};
        for(std::size_t n=0;n<dry.size();++n)dry[n]=source[n%period];
        std::uint64_t frames=0;
        const bool timeEffect=scenario.module==Fx||scenario.module==DigitalFx||scenario.module==Reverb96;
        const int settleBlocks=timeEffect?960:384; // Whole signal periods; at least 1.024 seconds.
        render(*rack,controls,source,settleBlocks,frames);
        render(*rack,controls,source,captureSize/blockSize,frames,&before);
        const auto& parameter=metadata(scenario.module).params[static_cast<std::size_t>(scenario.parameter)];

        // The real control atomic changes between 128-frame callback blocks.
        // The slot stays enabled, on CH8 / slot3, with no module reload or mute.
        REQUIRE(slot.enabled.load()&&slot.type.load()==scenario.module);
        slot.params[static_cast<std::size_t>(scenario.parameter)].store(scenario.after,std::memory_order_release);
        render(*rack,controls,source,settleBlocks,frames);
        render(*rack,controls,source,captureSize/blockSize,frames,&after);
        REQUIRE(slot.enabled.load()&&slot.type.load()==scenario.module);
        const double editDifference=difference(before,after),effectDifference=difference(after,dry);
        REQUIRE(editDifference>.0005);
        REQUIRE(effectDifference>.0005);
        if(scenario.module==FeedbackGuard||scenario.module==AutoFeedback){
            REQUIRE(!slot.feedbackArmed.load());
            REQUIRE(rms(after)<rms(before)*.2); // Manual -18 dB notch really attenuates its center tone.
        }

        slot.enabled.store(false,std::memory_order_release);
        render(*rack,controls,source,period/blockSize,frames); // Allow the 64-sample bypass ramp.
        render(*rack,controls,source,captureSize/blockSize,frames,&bypassed);
        const double bypassDifference=difference(bypassed,dry);
        REQUIRE(bypassDifference==0);
        std::cout<<metadata(scenario.module).name<<" | "<<parameter.name<<' '
                 <<scenario.before<<" -> "<<scenario.after<<" | "<<rms(dry)<<" | "<<rms(before)
                 <<" | "<<rms(after)<<" | "<<editDifference<<" | "<<effectDifference<<" | "<<bypassDifference<<'\n';
    }
    for(int module=1;module<=moduleCount;++module)REQUIRE(covered[static_cast<std::size_t>(module)]);
    std::cout<<"PASS: all 17 native processors change real audio at CH8 / INSERT3, "
               "respond to live parameters, leave other channels isolated, and bypass to exact dry\n";
}
