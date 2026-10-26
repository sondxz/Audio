// Eight-return offline integration acceptance. No JUCE or audio device is opened.
// Timing is deliberately not asserted: scheduler/load varies between machines.
#include "../src/dsp/MixDspCore.h"
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>
#include <vector>

namespace { bool watchingAllocations=false;std::size_t realtimeAllocations=0; }
void* operator new(std::size_t size){if(watchingAllocations)++realtimeAllocations;if(auto* p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t size){return ::operator new(size);}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
#define REQUIRE(x) do{if(!(x)){watchingAllocations=false;std::cerr<<"FAILED line "<<__LINE__<<": " #x "\n";std::exit(1);}}while(false)

namespace {
using namespace lah;
constexpr int channels=8,frames=128,blocks=128,totalFrames=frames*blocks;
constexpr double rate=48000;
using Block=std::array<std::array<float,frames>,channels>;
using Recording=std::vector<float>;

float stimulus(int channel,int frame)noexcept{
    const double t=frame/rate;
    const double envelope=(frame%4096)<3072?1.0:.25;
    return static_cast<float>(envelope*(.1*std::sin(2*builtin::pi*(73+17*channel)*t)
        +.13*std::sin(2*builtin::pi*(677+61*channel)*t)
        +.04*std::sin(2*builtin::pi*(5153+19*channel)*t))+(frame==channel*13?.17:0));
}
int destination(int source)noexcept{return (source+3)%channels;}
float routeWeight(int source)noexcept{return (source&1)?-.7f:.6f;}

struct Rig{
    dsp::Controls controls;
    builtin::Controls inputControls,outputControls;
    builtin::Rack inputRack,outputRack;
    dsp::Mixer mixer{controls,&inputRack,&inputControls,&outputRack,&outputControls};
    Block input{},output{};
    std::array<const float*,channels> inputs{};
    std::array<float*,channels> outputs{};
    Rig(){
        for(auto& value:controls.route)value.store(0);
        for(int ch=0;ch<channels;++ch){
            controls.route[destination(ch)*dsp::channels+ch].store(routeWeight(ch));
            inputs[ch]=input[ch].data();outputs[ch]=output[ch].data();
        }
        mixer.prepare(rate);
    }
    void process(bool enabled=true){
        watchingAllocations=true;
        mixer.process(inputs.data(),channels,outputs.data(),channels,frames,enabled);
        watchingAllocations=false;
        for(const auto& channel:output)for(float sample:channel)REQUIRE(std::isfinite(sample)&&std::abs(sample)<=1);
    }
    void restart(){process(false);for(const auto& channel:output)for(float sample:channel)REQUIRE(sample==0);}
    void clearSlots(){
        restart();
        // The direct Rack reference does not set Mixer's wasEnabled flag.
        // Start each independent configuration from a fresh native epoch too.
        // The PANIC lifecycle below uses restart() alone, without these resets.
        inputRack.reset();outputRack.reset();
        for(auto* rack:{&inputControls,&outputControls})for(auto& channel:rack->slots)
            for(auto& slot:channel)builtin::configureSlot(slot,builtin::None,false);
    }
    void fill(int block,int onlySource=-1){
        for(int ch=0;ch<channels;++ch)for(int n=0;n<frames;++n)
            input[ch][n]=onlySource==-1||onlySource==ch?stimulus(ch,block*frames+n):0;
    }
    void record(Recording& recording,int onlySource=-1,bool directReference=false){
        restart();
        for(int block=0;block<blocks;++block){
            fill(block,onlySource);
            if(!directReference)process();
            else{
                // A separate evaluation of the insert -> route -> insert graph,
                // without Mixer::process. This detects an integration order error.
                watchingAllocations=true;
                inputRack.beginBlock(inputControls);outputRack.beginBlock(outputControls);
                for(int n=0;n<frames;++n){
                    for(int ch=0;ch<channels;++ch){
                        const float pre=inputRack.processSample(ch,input[ch][n]);
                        const int out=destination(ch);
                        output[out][n]=std::clamp(outputRack.processSample(out,pre*routeWeight(ch)),-1.0f,1.0f);
                    }
                }
                watchingAllocations=false;
            }
            for(int ch=0;ch<channels;++ch)for(int n=0;n<frames;++n)
                recording[static_cast<std::size_t>(ch*totalFrames+block*frames+n)]=output[ch][n];
        }
    }
};

void configure(builtin::SlotControl& slot,int module,int channel){
    builtin::configureSlot(slot,module,true);
    // Use audible settings, including the two feedback modules' manual filters.
    // Automatic cut efficacy is covered separately by feedback_suppression tests.
    if(module==builtin::FeedbackGuard||module==builtin::AutoFeedback){slot.params[0].store(677+61.0f*channel);slot.params[1].store(-9);}
    if(module==builtin::Equalizer){slot.params[3].store(-6);if(channel==7)builtin::setExtendedEqEnabled(slot,true);}
    if(module==builtin::Fx){slot.params[0].store(40);slot.params[2].store(.6f);}
    if(module==builtin::Reverb96){slot.params[0].store(40);slot.params[1].store(2);}
}

void allModulesEightReturns(Rig& rig,Recording& all,Recording& isolated){
    for(bool outputDomain:{false,true})for(int module=1;module<=builtin::moduleCount;++module){
        rig.clearSlots();
        auto& controls=outputDomain?rig.outputControls:rig.inputControls;
        for(int source=0;source<channels;++source){
            const int channel=outputDomain?destination(source):source;
            configure(controls[channel][0],module,source);
        }
        rig.record(all);
        for(int source:{0,7}){
            rig.record(isolated,source);
            double energy=0;
            for(int out=0;out<channels;++out)for(int n=0;n<totalFrames;++n){
                const auto index=static_cast<std::size_t>(out*totalFrames+n);
                if(out==destination(source)){
                    REQUIRE(std::abs(isolated[index]-all[index])<2e-6f);
                    energy+=static_cast<double>(isolated[index])*isolated[index];
                }else REQUIRE(isolated[index]==0);
            }
            REQUIRE(energy>.001); // Prevent a silent/missing path from passing parity.
        }
        // Bypass is tested through the complete route on all eight channels.
        for(auto& channel:controls.slots)for(auto& slot:channel)slot.enabled.store(false);
        rig.fill(0);rig.process();
        for(int source=0;source<channels;++source)for(int n=64;n<frames;++n)
            REQUIRE(std::abs(rig.output[destination(source)][n]-rig.input[source][n]*routeWeight(source))<2e-6f);
    }
}

void dualFourSlotComposition(Rig& rig,Recording& actual,Recording& reference){
    // Five rotations visit every catalog module in a four-slot chain, with both
    // input and output racks active simultaneously on every physical return.
    for(int group=0;group<5;++group){
        rig.clearSlots();
        for(int ch=0;ch<channels;++ch)for(int slot=0;slot<builtin::slotCount;++slot){
            configure(rig.inputControls[ch][slot],1+(group*4+slot)%builtin::moduleCount,ch);
            configure(rig.outputControls[ch][slot],1+(group*4+3-slot+7)%builtin::moduleCount,ch);
        }
        rig.record(actual);rig.record(reference,-1,true);
        double energy=0;
        for(std::size_t n=0;n<actual.size();++n){
            REQUIRE(std::isfinite(actual[n])&&std::isfinite(reference[n]));
            if(std::abs(actual[n]-reference[n])>=3e-6f){
                std::cerr<<"Composition group="<<group<<" channel="<<n/totalFrames<<" frame="<<n%totalFrames
                    <<" actual="<<actual[n]<<" reference="<<reference[n]<<'\n';
                REQUIRE(false);
            }
            energy+=static_cast<double>(actual[n])*actual[n];
        }
        REQUIRE(energy>.001);
    }
}

void heavyTailPanicLifecycle(Rig& rig){
    rig.clearSlots();
    for(int ch=0;ch<channels;++ch)for(auto* controls:{&rig.inputControls,&rig.outputControls}){
        configure((*controls)[ch][0],builtin::Fx,ch);
        configure((*controls)[ch][1],builtin::Reverb96,ch);
        configure((*controls)[ch][2],builtin::DigitalFx,ch);
        configure((*controls)[ch][3],builtin::Dynamics,ch);
    }
    for(int block=0;block<192;++block){rig.fill(block);rig.process();}
    for(auto& channel:rig.input)channel.fill(0);
    double tailEnergy=0;
    for(int block=0;block<16;++block){rig.process();for(const auto& channel:rig.output)for(float sample:channel)tailEnergy+=static_cast<double>(sample)*sample;}
    REQUIRE(tailEnergy>.001);
    rig.restart();
    // The old tail would still be audible for seconds without epoch invalidation.
    for(int block=0;block<320;++block){rig.process();for(const auto& channel:rig.output)for(float sample:channel)REQUIRE(sample==0);}
    rig.fill(0);rig.process();
    float resumedPeak=0;for(const auto& channel:rig.output)for(float sample:channel)resumedPeak=std::max(resumedPeak,std::abs(sample));
    REQUIRE(resumedPeak>.001f);
}
}

int main(){
    auto rig=std::make_unique<Rig>();
    Recording actual(static_cast<std::size_t>(channels*totalFrames)),reference(actual.size());
    for(int module=1;module<=lah::builtin::moduleCount;++module)(void)lah::builtin::metadata(module);
    allModulesEightReturns(*rig,actual,reference);
    dualFourSlotComposition(*rig,actual,reference);
    heavyTailPanicLifecycle(*rig);
    REQUIRE(realtimeAllocations==0);
    std::cout<<"PASS: 17 native modules x 8 returns, input/output domains, isolated CH1/CH8 parity, "
        "permuted signed routing, bypass, five dual 8x4 chain compositions, tail PANIC/re-arm; "
        "zero processing allocations. Offline only; no show/hardware certification.\n";
}
