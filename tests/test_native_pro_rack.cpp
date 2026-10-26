// Exercises the actual native Rack bridge, not only standalone Pro processors.
#include "../src/dsp/BuiltinSceneState.h"
#include "../src/ui/RackOperations.h"
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <new>

namespace {bool watching=false;std::size_t allocations=0;}
void* operator new(std::size_t size){if(watching)++allocations;if(auto* p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t size){return ::operator new(size);}
void operator delete(void* p)noexcept{std::free(p);}void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
#define REQUIRE(x) do{if(!(x)){watching=false;std::cerr<<"FAILED line "<<__LINE__<<": " #x "\n";std::exit(1);}}while(false)

namespace {
using namespace lah;
constexpr int chCount=8,blockFrames=128;
constexpr double fs=48000,pi=3.14159265358979323846;
struct Harness{
    builtin::Controls controls;builtin::Rack rack;std::size_t cursor=0;
    explicit Harness(double rate=fs){rack.prepare(rate,blockFrames);}
    void reset(){rack.reset();cursor=0;}
    void clear(){for(auto& channel:controls.slots)for(auto& slot:channel)builtin::configureSlot(slot,builtin::None,false);reset();}
    void nextBlock(){cursor=0;}
    std::array<float,chCount> tick(const std::array<float,chCount>& input){
        std::array<float,chCount> result{};watching=true;
        if(cursor%blockFrames==0)rack.beginBlock(controls);
        for(int ch=0;ch<chCount;++ch)result[ch]=rack.processSample(ch,input[ch]);
        ++cursor;watching=false;
        for(float value:result)REQUIRE(std::isfinite(value)&&std::abs(value)<=16);
        return result;
    }
    float sample(int channel,float value){std::array<float,chCount> input{};input[channel]=value;return tick(input)[channel];}
    float constant(int channel,float value,int count){float result=0;for(int n=0;n<count;++n)result=sample(channel,value);return result;}
    void publish(){watching=true;rack.beginBlock(controls);watching=false;cursor=0;}
};
digitalfx::Snapshot echo(){
    auto value=digitalfx::defaults();value.enabled=true;value.params[digitalfx::EchoMix]=100;
    value.params[digitalfx::EchoTimeMs]=5;value.params[digitalfx::EchoFeedback]=0;
    value.params[digitalfx::EchoLowCut]=20;value.params[digitalfx::EchoHighCut]=18000;
    value.params[digitalfx::ReverbOn]=0;return value;
}
dynamicspro::Snapshot dynamicsOff(){
    dynamicspro::Snapshot value;value.enabled=true;
    value.params[dynamicspro::GateOn]=value.params[dynamicspro::CompOn]=value.params[dynamicspro::LimiterOn]=0;return value;
}
void setDigital(Harness& h,int channel,int slot,digitalfx::Snapshot value){
    builtin::configureSlot(h.controls[channel][slot],builtin::DigitalFx,true);digitalfx::recall(h.controls[channel][slot].digitalFxPro,value);
}
void setDynamics(Harness& h,int channel,int slot,dynamicspro::Snapshot value){
    builtin::configureSlot(h.controls[channel][slot],builtin::Dynamics,true);dynamicspro::recall(h.controls[channel][slot].dynamicsPro,value);
}
void actualEchoAndReverb(Harness& h){
    h.clear();
    for(int ch=0;ch<chCount;++ch){auto value=echo();value.params[digitalfx::EchoTimeMs]=5.0f+ch;setDigital(h,ch,3,value);}
    // Eight simultaneous echoes have eight different causal arrival times.
    std::array<float,chCount> peaks{};
    for(int n=0;n<800;++n){
        std::array<float,chCount> input{};if(n==0)input.fill(.5f);const auto actual=h.tick(input);
        for(int ch=0;ch<chCount;++ch){const int arrival=240+48*ch;if(n<arrival)REQUIRE(actual[ch]==0);if(n==arrival)REQUIRE(actual[ch]>.4f);peaks[ch]=std::max(peaks[ch],std::abs(actual[ch]));}
    }
    for(float peak:peaks)REQUIRE(peak>.4f);
    h.reset();
    for(int n=0;n<12000;++n){std::array<float,chCount> input{};if(n==0)input[7]=.5f;const auto actual=h.tick(input);for(int ch=0;ch<7;++ch)REQUIRE(actual[ch]==0);}

    h.clear();auto settings=echo();setDigital(h,7,3,settings);
    for(int n=0;n<128;++n)REQUIRE(h.sample(7,n==0?.5f:0)==0);
    settings.params[digitalfx::EchoMasterDb]=6;digitalfx::recall(h.controls[7][3].digitalFxPro,settings);
    for(int n=128;n<240;++n)REQUIRE(h.sample(7,0)==0);
    const double expected=.5*std::exp(-2*pi*20/fs)*(1-std::exp(-2*pi*18000/fs))*std::pow(10.,6./20);
    REQUIRE(std::abs(h.sample(7,0)-expected)<2e-7); // Gain edit retained the pending echo.

    h.clear();auto verb=digitalfx::defaults();verb.enabled=true;verb.params[digitalfx::EchoOn]=0;
    verb.params[digitalfx::ReverbMix]=100;verb.params[digitalfx::PreDelayMs]=20;
    verb.params[digitalfx::Diffusion]=0;verb.params[digitalfx::RoomSize]=.4f;setDigital(h,7,3,verb);
    const int arrival=960+static_cast<int>(std::lround(fs*.0297*verb.params[digitalfx::RoomSize]));
    for(int n=0;n<arrival;++n)REQUIRE(h.sample(7,n==0?.5f:0)==0);
    REQUIRE(h.sample(7,0)==.125f);
    double tail=0;for(int n=0;n<12000;++n){const float y=h.sample(7,0);tail+=static_cast<double>(y)*y;}REQUIRE(tail>.001);
    verb.params[digitalfx::PreDelayMs]=120;verb.params[digitalfx::RoomSize]=1;
    digitalfx::recall(h.controls[7][3].digitalFxPro,verb);h.nextBlock();
    for(int n=0;n<48000;++n){const float y=h.sample(7,0);if(n>=64)REQUIRE(y==0);}
}
void modeSwitchesAndBypass(Harness& h){
    h.clear();auto setting=echo();setting.params[digitalfx::EchoFeedback]=80;setting.params[digitalfx::EchoTimeMs]=20;
    setting.params[digitalfx::ReverbOn]=1;setting.params[digitalfx::ReverbMix]=80;setDigital(h,7,2,setting);
    auto& slot=h.controls[7][2];slot.params[0].store(4);slot.params[1].store(32);slot.params[2].store(100);slot.params[3].store(10);slot.params[4].store(.7f);
    for(int n=0;n<24000;++n)(void)h.sample(7,static_cast<float>(.2*std::sin(2*pi*997*n/fs)));
    double tail=0;for(int n=0;n<256;++n){const auto value=h.sample(7,0);tail+=static_cast<double>(value)*value;}REQUIRE(tail>.0001);
    slot.digitalFxPro.enabled.store(false);h.nextBlock();
    for(int n=0;n<48000;++n){const float y=h.sample(7,0);if(n>=64)REQUIRE(y==0);}
    // Now leave real legacy quantizer/echo data in the same stamped allocation.
    for(int n=0;n<12000;++n)(void)h.sample(7,.3f);
    for(int n=0;n<256;++n)(void)h.sample(7,0);
    slot.digitalFxPro.enabled.store(true);h.nextBlock();
    for(int n=0;n<48000;++n){const float y=h.sample(7,0);if(n>=64)REQUIRE(y==0);}
    h.reset();
    REQUIRE(h.sample(7,.5f)==0); // Pro is fully wet, unlike immediate legacy quantization.
    for(int n=1;n<960;++n)REQUIRE(h.sample(7,0)==0);
    REQUIRE(h.sample(7,0)>.4f);
    slot.enabled.store(false);h.nextBlock();
    for(int n=0;n<256;++n){const auto y=h.sample(7,.1234567f);if(n>=64)REQUIRE(y==.1234567f);}
    slot.enabled.store(true);h.nextBlock();
    for(int n=0;n<256;++n)(void)h.sample(7,0);
    h.rack.resetChannel(7);h.nextBlock();for(int n=0;n<48000;++n)REQUIRE(h.sample(7,0)==0);
}
void actualDynamicsAndFinalLimiter(Harness& h){
    h.clear();auto value=dynamicsOff();value.params[dynamicspro::CompOn]=1;value.params[dynamicspro::CompThreshold]=-20;
    value.params[dynamicspro::CompRatio]=4;value.params[dynamicspro::CompKnee]=0;value.params[dynamicspro::CompAttack]=.1f;
    for(int ch=0;ch<chCount;++ch)setDynamics(h,ch,2,value);
    const float compressed=h.constant(7,.5f,48000);
    const double expected=.1*std::pow(5.,.25);REQUIRE(std::abs(compressed-expected)<2e-6);
    h.publish();auto reduction=h.rack.dynamicsProReduction(7,2);
    REQUIRE(reduction.generation>0&&std::abs(reduction.compressorDb-(20*std::log10(.5)-(-20))*.75)<.001);
    REQUIRE(reduction.gateDb==0&&reduction.limiterDb==0);REQUIRE(h.rack.dynamicsProReduction(0,2).compressorDb==0);
    value.params[dynamicspro::Wet]=25;dynamicspro::recall(h.controls[7][2].dynamicsPro,value);h.nextBlock();
    REQUIRE(std::abs(h.constant(7,.5f,256)-(.5*.75+expected*.25))<2e-6);
    h.rack.invalidateMeters();REQUIRE(h.rack.dynamicsProReduction(7,2).generation==0);
    h.publish();REQUIRE(h.rack.dynamicsProReduction(7,2).generation==0); // Completed old block cannot revive a PANIC readout.
    h.reset();REQUIRE(h.rack.dynamicsProReduction(7,2).generation==0);
    h.constant(7,0,128);h.publish();REQUIRE(h.rack.dynamicsProReduction(7,2).compressorDb==0);

    h.clear();value=dynamicsOff();value.params[dynamicspro::GateOn]=1;value.params[dynamicspro::GateThreshold]=-30;
    value.params[dynamicspro::GateRatio]=3;value.params[dynamicspro::GateRange]=30;value.params[dynamicspro::GateHold]=0;
    value.params[dynamicspro::GateRelease]=5;setDynamics(h,7,2,value);
    const float quiet=static_cast<float>(std::pow(10.,-50./20));const float gated=h.constant(7,quiet,48000);
    REQUIRE(std::abs(20*std::log10(gated)+80)<.001);h.publish();REQUIRE(std::abs(h.rack.dynamicsProReduction(7,2).gateDb-30)<.001);
    h.controls[7][2].enabled.store(false);REQUIRE(h.rack.dynamicsProReduction(7,2).generation==0);h.nextBlock();
    for(int n=0;n<256;++n){const float y=h.sample(7,.2f);if(n>=64)REQUIRE(y==.2f);}

    h.clear();value=dynamicsOff();value.params[dynamicspro::LimiterOn]=1;value.params[dynamicspro::LimiterCeiling]=-1;setDynamics(h,7,2,value);
    const double ceiling=std::pow(10.,-12./20);
    h.constant(7,1,256);value.params[dynamicspro::LimiterCeiling]=-12;value.params[dynamicspro::Output]=12;
    dynamicspro::recall(h.controls[7][2].dynamicsPro,value);h.nextBlock();
    // The Rack's parameter transition must not undo the processor's final guard.
    for(int n=0;n<1024;++n)REQUIRE(std::abs(h.sample(7,n&1?1.0f:-1.0f))<=ceiling+1e-7);
    h.publish();reduction=h.rack.dynamicsProReduction(7,2);REQUIRE(reduction.limiterDb>20&&reduction.compressorDb==0);
    REQUIRE(h.rack.dynamicsProReduction(-1,2).generation==0&&h.rack.dynamicsProReduction(7,4).generation==0);
    h.controls[7][2].enabled.store(false);REQUIRE(h.rack.dynamicsProReduction(7,2).generation==0);h.nextBlock();
    for(int n=0;n<256;++n){const float y=h.sample(7,1);if(n>=64)REQUIRE(y==1);}
}
void snapshotMoveAndLegacy(Harness& h){
    h.clear();auto fx=echo();fx.params[digitalfx::EchoMasterDb]=6;setDigital(h,7,1,fx);setDynamics(h,7,2,dynamicsOff());
    const auto source=builtin::snapshotSlot(h.controls[7][1]);builtin::recallSlot(h.controls[0][1],source);
    REQUIRE(builtin::snapshotSlot(h.controls[0][1]).type==builtin::DigitalFx);
    REQUIRE(digitalfx::equal(builtin::snapshotSlot(h.controls[0][1]).digitalFxPro,fx));
    ui::RackOperations operations;REQUIRE(operations.move(h.controls,7,1,1,false));
    REQUIRE(h.controls[7][2].digitalFxPro.enabled.load()&&h.controls[7][1].dynamicsPro.enabled.load());
    REQUIRE(operations.copy(h.controls,7,2)&&operations.paste(h.controls,1,3,false));
    REQUIRE(!operations.paste(h.controls,2,3,true));h.reset();
    for(int n=0;n<4000;++n){std::array<float,chCount> input{};if(n==0)input[0]=input[1]=input[7]=.5f;const auto actual=h.tick(input);REQUIRE(actual[0]==actual[1]&&actual[1]==actual[7]);}
    h.clear();builtin::configureSlot(h.controls[7][3],builtin::DigitalFx,true);auto& legacy=h.controls[7][3];
    legacy.params[0].store(4);legacy.params[1].store(1);legacy.params[2].store(100);legacy.params[4].store(0);
    REQUIRE(std::abs(h.sample(7,.23f)-.1625f)<1e-7);REQUIRE(!legacy.digitalFxPro.enabled.load());
    h.clear();builtin::configureSlot(h.controls[7][3],builtin::Dynamics,true);auto& legacyDynamics=h.controls[7][3];
    legacyDynamics.params[0].store(0);legacyDynamics.params[1].store(1);legacyDynamics.params[4].store(-100);legacyDynamics.params[5].store(6);
    REQUIRE(std::abs(h.sample(7,.1f)-.1f*builtin::gain(6))<1e-7);REQUIRE(!legacyDynamics.dynamicsPro.enabled.load());
}
void liveBoundsAndFlags(){
    for(double rate:{8000.,48000.,192000.}){
        auto h=std::make_unique<Harness>(rate);
        for(int ch=0;ch<chCount;++ch){setDigital(*h,ch,1,echo());setDynamics(*h,ch,3,dynamicsOff());}
        for(int edit=0;edit<24;++edit){
            for(int ch=0;ch<chCount;++ch){
                auto& fx=h->controls[ch][1].digitalFxPro;auto& dyn=h->controls[ch][3].dynamicsPro;
                fx.enabled.store((edit%7)!=0);dyn.enabled.store((edit%5)!=0);
                for(int n=0;n<digitalfx::parameterCount;++n){const auto& p=digitalfx::specs()[static_cast<std::size_t>(n)];fx.params[n].store(edit%3==0?p.minimum:edit%3==1?p.maximum:std::numeric_limits<float>::quiet_NaN());}
                for(int n=0;n<dynamicspro::parameterCount;++n){const auto& p=dynamicspro::parameterSpecs()[static_cast<std::size_t>(n)];dyn.params[n].store(edit%3==0?p.minimum:edit%3==1?p.maximum:std::numeric_limits<float>::infinity());}
            }
            h->nextBlock();
            for(int n=0;n<256;++n){std::array<float,chCount> input{};for(int ch=0;ch<chCount;++ch)input[ch]=n%31==0?std::numeric_limits<float>::quiet_NaN():static_cast<float>(.1*std::sin(2*pi*(997+ch*41)*n/rate));(void)h->tick(input);}
        }
        h->reset();for(int n=0;n<2048;++n){const auto actual=h->tick({});for(float sample:actual)REQUIRE(sample==0);}
    }
}
}
int main(){
    auto h=std::make_unique<Harness>();actualEchoAndReverb(*h);modeSwitchesAndBypass(*h);
    actualDynamicsAndFinalLimiter(*h);snapshotMoveAndLegacy(*h);liveBoundsAndFlags();REQUIRE(allocations==0);
    std::cout<<"PASS: actual native Rack Pro echo/reverb, shared-stamp mode/layout changes, retained gain-edit tails, Dynamics analytic ratio/gate/final limiter+GR epochs, eight channels, snapshot/move/copy/legacy/bypass/reset, live bounds and0 callback allocations. Offline only.\n";
}
