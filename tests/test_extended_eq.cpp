#include "../src/dsp/BuiltinSceneState.h"
#include <complex>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <new>

namespace {bool measureAllocations=false;std::size_t allocations=0;}
void* operator new(std::size_t size){if(measureAllocations)++allocations;if(auto* memory=std::malloc(size?size:1))return memory;throw std::bad_alloc();}
void* operator new[](std::size_t size){return ::operator new(size);}
void operator delete(void* memory)noexcept{std::free(memory);}
void operator delete[](void* memory)noexcept{std::free(memory);}
void operator delete(void* memory,std::size_t)noexcept{std::free(memory);}
void operator delete[](void* memory,std::size_t)noexcept{std::free(memory);}
#define REQUIRE(condition) do{if(!(condition)){std::cerr<<"FAIL line "<<__LINE__<<": " #condition "\n";std::exit(1);}}while(false)
using namespace lah::builtin;
namespace {
// Independent analog prototypes evaluated after prewarped bilinear mapping.
// Does not use the native coefficient generator or its recurrence.
double referenceDb(EqBandSnapshot band,double frequency,double rate){
    const double center=std::min(static_cast<double>(band.frequency),rate*.45);
    const auto s=std::complex<double>(0,2*rate*std::tan(pi*frequency/rate));
    const double w=2*rate*std::tan(pi*center/rate),A=std::pow(10.,band.gainDb/40.),q=band.q;
    const auto second=s*s;const double omega=w*w;
    std::complex<double> response;
    switch(band.type){
        case EqHighPass:response=second/(second+w/q*s+omega);break;
        case EqLowPass:response=omega/(second+w/q*s+omega);break;
        case EqLowShelf:response=A*(second+std::sqrt(A)*w/q*s+A*omega)/(A*second+std::sqrt(A)*w/q*s+omega);break;
        case EqHighShelf:response=A*(A*second+std::sqrt(A)*w/q*s+omega)/(second+std::sqrt(A)*w/q*s+A*omega);break;
        default:response=(second+w*A/q*s+omega)/(second+w/(A*q)*s+omega);break;
    }
    return 20*std::log10(std::abs(response));
}
double rms(Rack& rack,Controls& controls,double frequency,float amplitude=.01f){
    rack.beginBlock(controls);double sum=0;
    for(int frame=0;frame<72000;++frame){
        const float input=amplitude*static_cast<float>(std::sin(2*pi*frequency*frame/48000));
        const float output=rack.processSample(7,input);
        REQUIRE(rack.processSample(0,input)==input);
        if(frame>=24000)sum+=static_cast<double>(output)*output;
    }
    return std::sqrt(sum/48000);
}
void disabledBands(SlotControl& slot){for(auto& band:slot.eqBands)band.enabled.store(false);}
}
int main(){
    // Every supported topology follows independent analog transfer functions at
    // multiple rates, frequencies and Q values, including shelves at +/- gain.
    for(double rate:{8000.,44100.,48000.,96000.,192000.})for(int type=EqBell;type<=EqLowPass;++type)
        for(float gainDb:{-12.f,0.f,9.f})for(float q:{.3f,.70710678f,3.f}){
            const EqBandSnapshot band{true,type,1000,gainDb,q};
            for(double frequency:{40.,200.,1000.,2000.}){
                const double difference=eqBandResponseDb(band,frequency,rate)-referenceDb(band,frequency,rate);
                if(std::abs(difference)>=2e-7)std::cerr<<"response rate="<<rate<<" type="<<type<<" gain="<<gainDb<<" q="<<q<<" f="<<frequency<<" difference="<<difference<<"\n";
                REQUIRE(std::abs(difference)<2e-7);
            }
        }
    auto controls=std::make_unique<Controls>();auto rack=std::make_unique<Rack>();rack->prepare(48000);
    auto& slot=controls->slots[7][2];configureSlot(slot,Equalizer,true);
    slot.params[0].store(70);slot.params[1].store(3);slot.params[2].store(1200);slot.params[3].store(6);slot.params[4].store(1.3f);slot.params[5].store(-2);
    const auto legacy=snapshotSlot(slot);REQUIRE(!legacy.extendedEq && !legacy.eqInitialized);
    const double legacyRms=rms(*rack,*controls,1200);
    REQUIRE(setExtendedEqEnabled(slot,true));
    const double convertedRms=rms(*rack,*controls,1200);
    REQUIRE(std::abs(20*std::log10(convertedRms/legacyRms))<.0001);
    REQUIRE(slot.eqInitialized.load() && slot.extendedEq.load());

    // All eight independent DSP bands are reachable, including the last band.
    for(int band=0;band<eqBandCount;++band){
        disabledBands(slot);recallEqBand(slot.eqBands[static_cast<std::size_t>(band)],{true,EqBell,1000,6,1});rack->reset();
        const auto level=rms(*rack,*controls,1000);
        REQUIRE(std::abs(20*std::log10(level/(.01/std::sqrt(2.0)))-6)<.001);
    }
    disabledBands(slot);recallEqBand(slot.eqBands[7],{true,EqHighPass,1000,0,.70710678f});rack->reset();
    REQUIRE(std::abs(20*std::log10(rms(*rack,*controls,1000)/(.01/std::sqrt(2.0)))+3.0103)<.001);
    recallEqBand(slot.eqBands[7],{true,EqLowPass,1000,0,.70710678f});rack->reset();
    REQUIRE(std::abs(20*std::log10(rms(*rack,*controls,1000)/(.01/std::sqrt(2.0)))+3.0103)<.001);
    for(int type:{EqLowShelf,EqHighShelf}){
        const EqBandSnapshot band{true,type,1000,type==EqLowShelf?12.0f:-9.0f,.70710678f};
        recallEqBand(slot.eqBands[7],band);rack->reset();const double probe=type==EqLowShelf?40:2000;
        REQUIRE(std::abs(20*std::log10(rms(*rack,*controls,probe)/(.01/std::sqrt(2.0)))-referenceDb(band,probe,48000))<.001);
    }

    // A live band gain edit uses the existing bounded transition and does not
    // allocate; bypass reaches the bit-exact dry signal after its transition.
    disabledBands(slot);recallEqBand(slot.eqBands[7],{true,EqBell,1000,0,1});rack->reset();rack->beginBlock(*controls);
    for(int n=0;n<1024;++n)(void)rack->processSample(7,.1f);
    measureAllocations=true;slot.eqBands[7].gainDb.store(12);rack->beginBlock(*controls);
    for(int n=0;n<8192;++n)REQUIRE(std::isfinite(rack->processSample(7,.1f)));
    slot.eqBands[7].enabled.store(false);rack->beginBlock(*controls);for(int n=0;n<64;++n)(void)rack->processSample(7,.123f);
    REQUIRE(rack->processSample(7,.123f)==.123f);
    slot.enabled.store(false);rack->beginBlock(*controls);for(int n=0;n<64;++n)(void)rack->processSample(7,.123f);
    REQUIRE(rack->processSample(7,.123f)==.123f);measureAllocations=false;REQUIRE(allocations==0);

    // Presets/copy/recall preserve both modes, including temporarily disabled
    // advanced settings, and never disarm an unrelated insert.
    slot.enabled.store(true);slot.eqBands[7].type.store(EqHighShelf);slot.eqBands[7].gainDb.store(-9);slot.eqBands[7].frequency.store(6300);
    setExtendedEqEnabled(slot,false);const auto saved=snapshotSlot(slot);REQUIRE(!saved.extendedEq && saved.eqInitialized);
    auto& other=controls->slots[0][0];configureSlot(other,AutoFeedback,true);other.feedbackArmed.store(true);
    recallSlot(controls->slots[6][1],saved);const auto copied=snapshotSlot(controls->slots[6][1]);
    REQUIRE(copied.type==saved.type && copied.params==saved.params && copied.eqBands==saved.eqBands && copied.eqInitialized);
    REQUIRE(other.feedbackArmed.load());setExtendedEqEnabled(controls->slots[6][1],true);
    REQUIRE(controls->slots[6][1].eqBands[7].gainDb.load()==-9);
    auto invalid=saved;invalid.extendedEq=true;invalid.eqBands[0]={true,999,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),0};
    const auto bounded=boundedSnapshot(invalid);REQUIRE(!bounded.eqBands[0].enabled && bounded.eqBands[0].type==EqBell);
    REQUIRE(bounded.eqBands[0].frequency==1000 && bounded.eqBands[0].gainDb==0 && bounded.eqBands[0].q==.1f);
    SlotSnapshot oldScene;oldScene.type=Equalizer;oldScene.enabled=true;oldScene.params=legacy.params;recallSlot(slot,oldScene);
    REQUIRE(!slot.extendedEq.load() && !slot.eqInitialized.load());
    REQUIRE(std::abs(20*std::log10(rms(*rack,*controls,1200)/legacyRms))<.0001);
    configureSlot(slot,VocalClean,true);REQUIRE(!slot.extendedEq.load() && !slot.eqInitialized.load());
    REQUIRE(!setExtendedEqEnabled(slot,true));

    // Runtime snapshot guards malicious/non-finite control values independently
    // of scene validation, at the supported maximum sample rate.
    configureSlot(slot,Equalizer,true);setExtendedEqEnabled(slot,true);rack->prepare(192000);
    for(auto& band:slot.eqBands){band.enabled.store(true);band.type.store(EqHighShelf);band.frequency.store(std::numeric_limits<float>::infinity());band.gainDb.store(999);band.q.store(std::numeric_limits<float>::quiet_NaN());}
    rack->beginBlock(*controls);for(int n=0;n<4096;++n)REQUIRE(std::isfinite(rack->processSample(7,n==0?.1f:0)));
    std::cout<<"PASS: eight-band EQ five filter types, independent analog references 8-192k, all eight audio bands, legacy conversion/scene compatibility, full slot snapshots, live no-allocation, isolation and invalid guards\n";
}
