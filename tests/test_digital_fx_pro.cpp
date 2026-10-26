#include "../src/dsp/DigitalFxPro.h"
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <new>
#include <vector>

namespace {bool watchAllocations=false;std::size_t allocations=0;}
void* operator new(std::size_t size){if(watchAllocations)++allocations;if(auto* p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t size){return ::operator new(size);}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
#define REQUIRE(x) do{if(!(x)){watchAllocations=false;std::cerr<<"FAILED line "<<__LINE__<<": " #x "\n";std::exit(1);}}while(false)

namespace {
using namespace lah::digitalfx;
constexpr double pi=3.14159265358979323846,rate=48000;
struct Storage{
    std::vector<float> values;std::vector<std::uint64_t> stamps;std::uint64_t epoch=1;
    explicit Storage(double fs):values(static_cast<std::size_t>(std::ceil(fs*.65))+16),stamps(values.size()){}
    void invalidate()noexcept{++epoch;}
    float read(std::size_t index)const noexcept{REQUIRE(index<values.size());return stamps[index]==epoch?values[index]:0;}
    void write(std::size_t index,float value)noexcept{REQUIRE(index<values.size());REQUIRE(std::isfinite(value));values[index]=value;stamps[index]=epoch;}
};
void configure(Processor& processor,Storage& storage,const Snapshot& settings,double fs=rate){
    watchAllocations=true;const bool changed=processor.configure(settings,fs);watchAllocations=false;
    if(changed)storage.invalidate();
}
Snapshot echoOnly(){auto s=defaults();s.enabled=true;s.params[ReverbOn]=0;s.params[EchoMix]=100;s.params[EchoTimeMs]=5;s.params[EchoFeedback]=0;s.params[EchoLowCut]=20;s.params[EchoHighCut]=18000;return s;}
Snapshot reverbOnly(){auto s=defaults();s.enabled=true;s.params[EchoOn]=0;s.params[ReverbMix]=100;s.params[PreDelayMs]=0;s.params[Diffusion]=0;s.params[EchoToReverb]=0;return s;}
double energy(const std::vector<float>& values,int start,int end){double sum=0;for(int n=start;n<end;++n)sum+=static_cast<double>(values[static_cast<std::size_t>(n)])*values[static_cast<std::size_t>(n)];return sum;}
std::vector<float> impulse(Snapshot settings,int length,double fs=rate){
    Storage storage(fs);Processor processor;configure(processor,storage,settings,fs);
    std::vector<float> result(static_cast<std::size_t>(length));watchAllocations=true;
    for(int n=0;n<length;++n){result[static_cast<std::size_t>(n)]=processor.tick(n==0?.5f:0,storage);REQUIRE(std::isfinite(result[static_cast<std::size_t>(n)]));}
    watchAllocations=false;return result;
}
void controlsAndDry(){
    Controls controls;REQUIRE(equal(snapshot(controls),defaults()));
    auto setting=echoOnly();setting.params[EchoFeedback]=300;setting.params[EchoLowCut]=std::numeric_limits<float>::quiet_NaN();
    recall(controls,setting);const auto copied=snapshot(controls);
    REQUIRE(copied.enabled&&copied.params[EchoFeedback]==85&&copied.params[EchoLowCut]==specs()[EchoLowCut].defaultValue);
    reset(controls);REQUIRE(equal(snapshot(controls),defaults()));
    Processor processor;Storage storage(rate);auto dry=defaults();dry.enabled=true;dry.params[EchoMix]=dry.params[ReverbMix]=0;
    configure(processor,storage,dry);
    const std::array<float,10> samples{{-0.0f,0,.25f,-.9f,1.5f,-8,16,-16,std::numeric_limits<float>::max(),std::numeric_limits<float>::denorm_min()}};
    watchAllocations=true;
    for(int pass=0;pass<200;++pass)for(float input:samples){const float output=processor.tick(input,storage);REQUIRE(std::memcmp(&input,&output,sizeof(float))==0);}
    watchAllocations=false;
    dry.enabled=false;configure(processor,storage,dry);REQUIRE(processor.tick(.7f,storage)==.7f);
    REQUIRE(processor.tick(std::numeric_limits<float>::quiet_NaN(),storage)==0);
}
void echoTimingAndFilterReference(){
    auto settings=echoOnly();const auto rendered=impulse(settings,1200);
    constexpr int delay=240;
    for(int n=0;n<delay;++n)REQUIRE(rendered[static_cast<std::size_t>(n)]==0);
    const double hp=std::exp(-2*pi*20/rate),lp=std::exp(-2*pi*18000/rate);
    REQUIRE(std::abs(rendered[delay]-.5*hp*(1-lp))<1e-7);
    // Independent complex transfer function of a one-pole high-pass/low-pass.
    // Echo delay has unit magnitude, so coherent tone RMS gives the filter gain.
    for(double hz:{100.0,1000.0,8000.0})for(bool darker:{false,true}){
        Processor processor;Storage storage(rate);settings.params[EchoLowCut]=darker?600.0f:20.0f;settings.params[EchoHighCut]=darker?1500.0f:18000.0f;
        configure(processor,storage,settings);double sum=0;watchAllocations=true;
        for(int n=0;n<96000;++n){const float y=processor.tick(static_cast<float>(.2*std::sin(2*pi*hz*n/rate)),storage);if(n>=48000)sum+=static_cast<double>(y)*y;}
        watchAllocations=false;
        const double a=std::exp(-2*pi*settings.params[EchoLowCut]/rate),b=std::exp(-2*pi*settings.params[EchoHighCut]/rate);
        const auto z=std::polar(1.0,-2*pi*hz/rate);
        const double expected=std::abs(a*(1.0-z)/(1.0-a*z)*(1-b)/(1.0-b*z));
        const double measured=std::sqrt(sum/48000)/(.2/std::sqrt(2.0));REQUIRE(std::abs(measured-expected)<2e-5);
    }
    settings=echoOnly();settings.params[EchoTimeMs]=40;
    const auto noFeedback=impulse(settings,96000);settings.params[EchoFeedback]=80;
    const auto withFeedback=impulse(settings,96000);
    REQUIRE(energy(withFeedback,3840,96000)>energy(noFeedback,3840,96000)*20);
    REQUIRE(energy(withFeedback,0,96000)>energy(noFeedback,0,96000)*1.5);
}
void reverbTimingDecayAndRouting(){
    auto settings=reverbOnly();settings.params[RoomSize]=.4f;
    const auto first=impulse(settings,12000);
    const int firstSample=static_cast<int>(std::lround(rate*.0297*settings.params[RoomSize]));
    for(int n=0;n<firstSample;++n)REQUIRE(first[static_cast<std::size_t>(n)]==0);
    REQUIRE(first[static_cast<std::size_t>(firstSample)]==.125f);
    settings.params[PreDelayMs]=20;const auto predelayed=impulse(settings,12000);
    for(int n=0;n<960;++n)REQUIRE(predelayed[static_cast<std::size_t>(n)]==0);
    for(int n=960;n<12000;++n)REQUIRE(predelayed[static_cast<std::size_t>(n)]==first[static_cast<std::size_t>(n-960)]);
    settings=reverbOnly();settings.params[DecaySeconds]=.3f;const auto shortDecay=impulse(settings,96000);
    settings.params[DecaySeconds]=3;const auto longDecay=impulse(settings,96000);
    REQUIRE(energy(longDecay,24000,72000)>energy(shortDecay,24000,72000)*100);
    settings.params[DampingHz]=500;const auto damped=impulse(settings,96000);
    REQUIRE(energy(damped,24000,72000)<energy(longDecay,24000,72000)*.5);
    settings.params[Diffusion]=100;const auto diffused=impulse(settings,12000);
    settings.params[Diffusion]=0;const auto plain=impulse(settings,12000);double difference=0;
    for(std::size_t n=0;n<plain.size();++n)difference+=std::abs(plain[n]-diffused[n]);REQUIRE(difference>.1);
    // The echo send feeds the reverb even when the echo's direct return is down.
    settings=reverbOnly();settings.params[EchoOn]=1;settings.params[EchoMix]=0;settings.params[EchoTimeMs]=20;
    settings.params[EchoToReverb]=0;const auto dryFeed=impulse(settings,24000);
    settings.params[EchoToReverb]=100;const auto echoFeed=impulse(settings,24000);difference=0;
    for(std::size_t n=0;n<dryFeed.size();++n)difference+=std::abs(dryFeed[n]-echoFeed[n]);REQUIRE(difference>.1);
}
void gainsDuckingAndContinuousState(){
    auto settings=echoOnly();settings.params[EchoTimeMs]=20;const auto base=impulse(settings,12000);
    settings.params[EchoMasterDb]=6;const auto louder=impulse(settings,12000);
    for(std::size_t n=0;n<base.size();++n)REQUIRE(std::abs(louder[n]-base[n]*std::pow(10.0,6.0/20))<2e-7);
    settings.params[OutputDb]=-6;const auto restored=impulse(settings,12000);
    for(std::size_t n=0;n<base.size();++n)REQUIRE(std::abs(restored[n]-base[n])<1e-7);
    settings=reverbOnly();const auto verb=impulse(settings,12000);settings.params[ReverbMasterDb]=-6;const auto quietVerb=impulse(settings,12000);
    for(std::size_t n=0;n<verb.size();++n)REQUIRE(std::abs(quietVerb[n]-verb[n]*std::pow(10.0,-6.0/20))<1e-7);

    settings=echoOnly();settings.params[EchoFeedback]=80;settings.params[EchoTimeMs]=100;
    std::array<double,2> voiced{},earlyTail{},lateTail{};
    for(int duck=0;duck<2;++duck){
        settings.params[Ducking]=duck?100.0f:0.0f;Processor processor;Storage storage(rate);configure(processor,storage,settings);watchAllocations=true;
        for(int n=0;n<96000;++n){
            const float y=processor.tick(n<48000?static_cast<float>(.4*std::sin(2*pi*500*n/rate)):0,storage);
            const double power=static_cast<double>(y)*y;
            if(n>=24000&&n<48000)voiced[duck]+=power;
            if(n>=48000&&n<52800)earlyTail[duck]+=power;
            if(n>=86400)lateTail[duck]+=power;
        }
        watchAllocations=false;
    }
    REQUIRE(voiced[1]<voiced[0]*.2);REQUIRE(earlyTail[1]<earlyTail[0]*.4);
    REQUIRE(lateTail[1]>lateTail[0]*.9); // Quiet intervals release the wet ducking.

    // A gain edit retains the causal echo history and does not request a layout reset.
    settings=echoOnly();Processor processor;Storage storage(rate);configure(processor,storage,settings);
    for(int n=0;n<120;++n)(void)processor.tick(n==0?.5f:0,storage);
    settings.params[EchoMasterDb]=6;REQUIRE(!processor.configure(settings,rate));
    for(int n=120;n<240;++n)REQUIRE(processor.tick(0,storage)==0);
    REQUIRE(processor.tick(0,storage)>.5f);
    const auto required=processor.requiredSamples();processor.reset();REQUIRE(processor.requiredSamples()==required);
    storage.invalidate();for(int n=0;n<24000;++n)REQUIRE(processor.tick(0,storage)==0);
}
void mixesCircuitSwitchesAndLayoutEdits(){
    auto echo=echoOnly();const auto echoWet=impulse(echo,12000);echo.params[EchoMix]=25;const auto echoQuarter=impulse(echo,12000);
    for(int n=0;n<12000;++n){const double expected=(n==0?.5*.75:0)+echoWet[static_cast<std::size_t>(n)]*.25;REQUIRE(std::abs(echoQuarter[static_cast<std::size_t>(n)]-expected)<1e-7);}
    auto verb=reverbOnly();const auto verbWet=impulse(verb,12000);verb.params[ReverbMix]=25;const auto verbQuarter=impulse(verb,12000);
    for(int n=0;n<12000;++n){const double expected=(n==0?.5*.75:0)+verbWet[static_cast<std::size_t>(n)]*.25;REQUIRE(std::abs(verbQuarter[static_cast<std::size_t>(n)]-expected)<1e-7);}
    auto both=defaults();both.enabled=true;both.params[EchoOn]=both.params[ReverbOn]=0;both.params[EchoMix]=both.params[ReverbMix]=100;
    Processor processor;Storage storage(rate);configure(processor,storage,both);
    for(int n=0;n<12000;++n){const float input=n%77==0?.3f:-.1f;const float output=processor.tick(input,storage);REQUIRE(std::memcmp(&input,&output,sizeof(float))==0);}
    both.params[OutputDb]=6;REQUIRE(!processor.configure(both,rate));
    REQUIRE(std::abs(processor.tick(.1f,storage)-.1*std::pow(10.,.3))<1e-7);
    // Only format/layout changes require external memory invalidation. A run of
    // every ordinary control edit must not silently discard ongoing echoes.
    for(int index=0;index<parameterCount;++index){
        auto edited=both;edited.params[static_cast<std::size_t>(index)]=specs()[static_cast<std::size_t>(index)].maximum;
        watchAllocations=true;const bool changed=processor.configure(edited,rate);watchAllocations=false;
        const bool structural=index==EchoTimeMs||index==PreDelayMs;
        // RoomSize already equals its maximum in both/defaults.
        REQUIRE(changed==structural);
        configure(processor,storage,both);
    }
    auto smaller=both;smaller.params[RoomSize]=.4f;REQUIRE(processor.configure(smaller,rate));storage.invalidate();
    smaller.enabled=false;REQUIRE(processor.configure(smaller,rate));storage.invalidate();
    smaller.enabled=true;REQUIRE(processor.configure(smaller,rate));storage.invalidate();
}
void boundsIsolationAndNoAllocations(){
    for(double fs:{8000.0,44100.0,48000.0,96000.0,192000.0}){
        Processor left,right;Storage leftStorage(fs),rightStorage(fs);auto settings=defaults();settings.enabled=true;
        for(bool maximum:{false,true}){
            for(int n=0;n<parameterCount;++n)settings.params[static_cast<std::size_t>(n)]=maximum?specs()[static_cast<std::size_t>(n)].maximum:specs()[static_cast<std::size_t>(n)].minimum;
            configure(left,leftStorage,settings,fs);configure(right,rightStorage,settings,fs);
            REQUIRE(left.requiredSamples()<=static_cast<std::size_t>(std::ceil(fs*.64)));
            REQUIRE(left.requiredSamples()<leftStorage.values.size());
            watchAllocations=true;
            for(int n=0;n<static_cast<int>(fs*.8);++n){
                const float input=n%4096==0?.7f:static_cast<float>(.25*std::sin(2*pi*997*n/fs));
                const float output=left.tick(input,leftStorage);REQUIRE(std::isfinite(output));
                REQUIRE(right.tick(0,rightStorage)==0);
            }
            watchAllocations=false;
            left.reset();leftStorage.invalidate();for(int n=0;n<1000;++n)REQUIRE(left.tick(0,leftStorage)==0);
        }
        for(float invalid:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()}){
            settings.params.fill(invalid);configure(left,leftStorage,settings,fs);watchAllocations=true;
            for(int n=0;n<4096;++n)REQUIRE(std::isfinite(left.tick(n&1?invalid:std::numeric_limits<float>::max(),leftStorage)));
            watchAllocations=false;
        }
    }
    Processor processor;auto settings=defaults();settings.enabled=true;
    REQUIRE(processor.configure(settings,std::numeric_limits<double>::quiet_NaN()));REQUIRE(processor.sampleRate()==48000);
    REQUIRE(processor.configure(settings,1));REQUIRE(processor.sampleRate()==8000);
    REQUIRE(processor.configure(settings,999999));REQUIRE(processor.sampleRate()==192000);
}
}
int main(){
    controlsAndDry();echoTimingAndFilterReference();reverbTimingDecayAndRouting();gainsDuckingAndContinuousState();mixesCircuitSwitchesAndLayoutEdits();boundsIsolationAndNoAllocations();
    REQUIRE(allocations==0);
    std::cout<<"PASS: mono Digital FX Pro18 controls; exact echo/predelay, independent filter reference, feedback/RT60/damping/diffusion, echo-to-reverb, masters, duck/recovery, dry bits, state continuity,8-192k bounds/isolation,0 processing allocations. Offline only.\n";
}
