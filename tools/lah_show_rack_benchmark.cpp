// Offline observations of actual Mixer/Rack work. Never opens audio hardware.
// A missed synthetic block budget is reported, never converted into a CTest assertion.
#include "../src/dsp/MixDspCore.h"
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <new>
#include <string>
#include <vector>

namespace {bool watchAllocations=false;std::size_t processingAllocations=0;}
void* operator new(std::size_t size){if(watchAllocations)++processingAllocations;if(auto* p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t size){return ::operator new(size);}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
void operator delete[](void* p,std::size_t)noexcept{std::free(p);}

namespace {
using namespace lah;
using Clock=std::chrono::steady_clock;
constexpr int channels=8,frames=128;
constexpr double rate=48000,budgetUs=frames/rate*1e6;
struct Scenario{std::string name;int module=0,slots=0,domain=0;bool liveEdits=false;};
struct Fixture{
    dsp::Controls mixerControls;
    builtin::Controls inputControls,outputControls;
    builtin::Rack inputRack,outputRack;
    dsp::Mixer mixer{mixerControls,&inputRack,&inputControls,&outputRack,&outputControls};
    std::array<std::array<float,frames>,channels> input{},output{};
    std::array<const float*,channels> inputs{};
    std::array<float*,channels> outputs{};
    std::uint64_t framePosition=0;
    Fixture(){
        for(int ch=0;ch<channels;++ch){inputs[ch]=input[ch].data();outputs[ch]=output[ch].data();}
        // Actual eight independent USB-style returns; other 24 channels absent.
        mixer.prepare(rate);
    }
    void reset(const Scenario& scenario){
        mixer.process(inputs.data(),channels,outputs.data(),channels,frames,false);
        inputRack.reset();outputRack.reset();
        for(auto* controls:{&inputControls,&outputControls})for(auto& channel:controls->slots)
            for(auto& slot:channel)builtin::configureSlot(slot,builtin::None,false);
        for(int domain=0;domain<2;++domain){
            if(scenario.domain!=2&&scenario.domain!=domain)continue;
            auto& controls=domain==0?inputControls:outputControls;
            for(int ch=0;ch<channels;++ch)for(int slot=0;slot<scenario.slots;++slot){
                const int module=scenario.module==0?1+(ch*4+slot+domain*7)%builtin::moduleCount:scenario.module;
                auto& control=controls[ch][slot];builtin::configureSlot(control,module,true);
                if(module==builtin::Equalizer){
                    builtin::setExtendedEqEnabled(control,true);
                    // All eight actual EQ sections, rather than the legacy four.
                    for(int band=0;band<builtin::eqBandCount;++band)
                        builtin::recallEqBand(control.eqBands[band],{true,builtin::EqBell,static_cast<float>(40*std::pow(2.2,band)),(band&1)?-3.0f:3.0f,1});
                }
                if(module==builtin::Reverb96)control.params[1].store(5);
                if(module==builtin::Fx)control.params[2].store(.8f);
                if(module==builtin::DigitalFx)control.params[4].store(.7f);
            }
        }
        framePosition=0;
    }
    void arm(){
        for(auto* controls:{&inputControls,&outputControls})for(int ch=0;ch<channels;++ch)
            for(auto& slot:(*controls)[ch]){
                const int module=slot.type.load();
                if(module==builtin::FeedbackGuard||module==builtin::AutoFeedback)slot.feedbackArmed.store(true);
            }
    }
    void fill(bool feedbackStress){
        for(int ch=0;ch<channels;++ch)for(int n=0;n<frames;++n){
            const double t=(framePosition+static_cast<std::uint64_t>(n))/rate;
            // Rising, then falling isolated tones exercise armed spectral/model
            // tracking; other cases use transients plus a non-clipping multitone.
            if(feedbackStress){
                const double phase=std::fmod(t,3.0);
                const double db=phase<2?-55+25*phase:-5-50*(phase-2);
                input[ch][n]=static_cast<float>(std::pow(10.,db/20)*std::sin(2*builtin::pi*(997+83*ch)*t));
            }else input[ch][n]=static_cast<float>(.13*std::sin(2*builtin::pi*(79+17*ch)*t)
                +.17*std::sin(2*builtin::pi*(697+47*ch)*t)+.035*std::sin(2*builtin::pi*(6157+23*ch)*t)
                +((framePosition+static_cast<std::uint64_t>(n))%4096==0?.08:0));
        }
        framePosition+=frames;
    }
    void edit(int block){
        // UI-style atomic edits consumed by the next real beginBlock. No reset,
        // mute, prepare or module replacement occurs in this measured scenario.
        for(auto* controls:{&inputControls,&outputControls})for(int ch=0;ch<channels;++ch)
            for(auto& slot:(*controls)[ch]){
                const int module=slot.type.load();if(module==builtin::None)continue;
                const auto& specification=builtin::metadata(module).params[static_cast<std::size_t>((block/16)%6)];
                slot.params[static_cast<std::size_t>((block/16)%6)].store((block/96)&1?specification.minimum:specification.maximum);
            }
    }
    double process(){
        watchAllocations=true;
        const auto start=Clock::now();
        mixer.process(inputs.data(),channels,outputs.data(),channels,frames,true);
        const auto finish=Clock::now();watchAllocations=false;
        return std::chrono::duration<double,std::micro>(finish-start).count();
    }
};
double percentile(const std::vector<double>& values,double fraction){
    const auto index=static_cast<std::size_t>(std::ceil(values.size()*fraction));
    return values[std::min(values.size()-1,index?index-1:0)];
}
}

int main(int argc,char** argv){
    double requestedSeconds=30;
    if(argc==3&&std::string(argv[1])=="--seconds"){
        char* end=nullptr;requestedSeconds=std::strtod(argv[2],&end);
        if(!end||*end||!std::isfinite(requestedSeconds)||requestedSeconds<30||requestedSeconds>60){std::cerr<<"--seconds requires 30..60\n";return 1;}
    }else if(argc!=1){std::cerr<<"Usage: lah_show_rack_benchmark [--seconds 30..60]\n";return 1;}
    std::vector<Scenario> scenarios;
    scenarios.push_back({"bypass_8returns",0,0,2,false});
    for(int slots:{1,4})for(int domain=0;domain<2;++domain)for(int module=1;module<=builtin::moduleCount;++module)
        scenarios.push_back({std::string(domain==0?"input_":"output_")+std::to_string(module)+"_8x"+std::to_string(slots),module,slots,domain,false});
    scenarios.push_back({"dual_mixed_8x4",0,4,2,false});
    scenarios.push_back({"dual_reverb_8x4",builtin::Reverb96,4,2,false});
    scenarios.push_back({"dual_aifb_armed_8x4",builtin::FeedbackGuard,4,2,false});
    scenarios.push_back({"dual_autofb_armed_8x4",builtin::AutoFeedback,4,2,false});
    scenarios.push_back({"dual_live_edits_8x4",0,4,2,true});
    auto fixture=std::make_unique<Fixture>();
    for(int module=1;module<=builtin::moduleCount;++module)(void)builtin::metadata(module);
    std::cout<<"OFFLINE ONLY: actual native Mixer/Rack, 48000 Hz, 128 samples, 8 input/8 output returns.\n"
        <<"Period budget 2666.666667 us; timed process excludes signal generation/scan, driver, UI, FIR and VST3.\n"
        <<"No device is opened. Not ASIO round-trip latency, xrun, 24-hour soak, acoustic safety or show certification.\n"
        <<"EQ uses all 8 bands. FB cases are armed after reset; long-decay/delay feedback settings are stressed.\n"
        <<"scenario,blocks,audio_seconds,p50_us,p99_us,max_us,max_budget_percent,over_budget,processing_allocations,nonfinite,peak,learned_notches\n";
    std::size_t totalMeasured=0,totalOver=0,totalNonfinite=0;
    double worst=0;std::string worstName;
    const auto overallStart=Clock::now();
    for(std::size_t scenarioIndex=0;scenarioIndex<scenarios.size();++scenarioIndex){
        const auto& scenario=scenarios[scenarioIndex];
        const auto scenarioStart=Clock::now();fixture->reset(scenario);
        const double remaining=std::max(.01,requestedSeconds-std::chrono::duration<double>(Clock::now()-overallStart).count());
        const double slice=remaining/static_cast<double>(scenarios.size()-scenarioIndex);
        const bool feedbackStress=scenario.module==builtin::FeedbackGuard||scenario.module==builtin::AutoFeedback;
        for(int warm=0;warm<16;++warm){fixture->fill(feedbackStress);(void)fixture->process();if(warm==0)fixture->arm();}
        const auto allocationsBefore=processingAllocations;
        std::vector<double> observations;observations.reserve(8192);
        std::size_t over=0,nonfinite=0;float peak=0;int block=0;
        do{
            fixture->fill(feedbackStress);
            if(scenario.liveEdits&&block%16==0)fixture->edit(block);
            const double us=fixture->process();observations.push_back(us);if(us>budgetUs)++over;
            for(const auto& channel:fixture->output)for(float sample:channel){
                if(!std::isfinite(sample))++nonfinite;else peak=std::max(peak,std::abs(sample));
            }
            ++block;
        }while(observations.size()<64||std::chrono::duration<double>(Clock::now()-scenarioStart).count()<slice);
        std::sort(observations.begin(),observations.end());
        int notches=0;
        for(auto* rack:{&fixture->inputRack,&fixture->outputRack})for(int ch=0;ch<channels;++ch)
            for(int slot=0;slot<builtin::slotCount;++slot)notches+=rack->feedbackStatus(ch,slot).notchCount;
        const auto allocations=processingAllocations-allocationsBefore;
        std::cout<<scenario.name<<','<<observations.size()<<','<<std::fixed<<std::setprecision(6)
            <<observations.size()*frames/rate<<','<<percentile(observations,.5)<<','<<percentile(observations,.99)<<','
            <<observations.back()<<','<<observations.back()/budgetUs*100<<','<<over<<','<<allocations<<','<<nonfinite<<','<<peak<<','<<notches<<'\n';
        totalMeasured+=observations.size();totalOver+=over;totalNonfinite+=nonfinite;
        if(observations.back()>worst){worst=observations.back();worstName=scenario.name;}
    }
    const double wallSeconds=std::chrono::duration<double>(Clock::now()-overallStart).count();
    std::cout<<"SUMMARY scenarios="<<scenarios.size()<<" measured_blocks="<<totalMeasured<<" wall_seconds="<<wallSeconds
        <<" over_budget="<<totalOver<<" worst_scenario="<<worstName<<" worst_us="<<worst
        <<" all_processing_allocations="<<processingAllocations<<" nonfinite="<<totalNonfinite<<'\n';
    std::cout<<(totalOver?"OBSERVED DEADLINE EXCEEDANCES: do not infer show readiness from this run.\n":"No measured synthetic exceedance; actual driver/system headroom remains untested.\n");
    return processingAllocations||totalNonfinite?2:0;
}
