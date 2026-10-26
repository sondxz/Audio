#include "../src/dsp/DynamicsPro.h"
#include <cstdlib>
#include <iostream>
#include <new>
#include <limits>
#include <cstring>
namespace {bool countAllocations=false;std::size_t allocations=0;}
void* operator new(std::size_t size){if(countAllocations)++allocations;if(auto* p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t size){return ::operator new(size);}
void operator delete(void* p)noexcept{std::free(p);}void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
#define REQUIRE(c) do{if(!(c)){std::cerr<<"FAIL line "<<__LINE__<<": " #c "\n";std::exit(1);}}while(false)
using namespace lah::dynamicspro;
namespace {
Snapshot off(){Snapshot s;s.enabled=true;s.params[GateOn]=s.params[CompOn]=s.params[LimiterOn]=0;return s;}
float gain(double db){return static_cast<float>(std::pow(10.,db/20));}
int frames(double rate,double milliseconds){return static_cast<int>(std::lround(rate*milliseconds*.001));}
float run(Processor& p,float input,int count){float result=0;for(int i=0;i<count;++i)result=p.tick(input);return result;}
void schemaBounds(){
    Controls controls;auto state=snapshot(controls);REQUIRE(!state.enabled && state.params==defaults());
    state.enabled=true;for(int i=0;i<parameterCount;++i)state.params[static_cast<std::size_t>(i)]=std::numeric_limits<float>::quiet_NaN();
    recall(controls,state);REQUIRE(snapshot(controls).params==defaults());
    for(int i=0;i<parameterCount;++i){const auto index=static_cast<std::size_t>(i);state.params[index]=std::numeric_limits<float>::max();}
    const auto high=bounded(state);for(int i=0;i<parameterCount;++i)REQUIRE(high.params[static_cast<std::size_t>(i)]==parameterSpecs()[static_cast<std::size_t>(i)].maximum);
    state.params[GateOn]=.49f;state.params[CompOn]=.5f;state.params[LimiterOn]=-.5f;const auto flags=bounded(state);REQUIRE(flags.params[GateOn]==0 && flags.params[CompOn]==1 && flags.params[LimiterOn]==0);
    reset(controls);REQUIRE(snapshot(controls)==Snapshot{});static_assert(Processor::latencySamples==0);static_assert(sizeof(Processor)<1024);
}
void analyticCurvesAndTimes(){
    for(double rate:{8000.,44100.,48000.,96000.,192000.}){
        auto s=off();s.params[CompOn]=1;s.params[CompThreshold]=-20;s.params[CompRatio]=4;s.params[CompKnee]=0;s.params[CompAttack]=10;s.params[CompRelease]=120;
        Processor p;p.configure(s,rate);countAllocations=true;
        const auto attackCount=frames(rate,10);run(p,1,attackCount);
        const double expectedAttack=15*(1-std::exp(-static_cast<double>(attackCount)/(.01*rate)));
        REQUIRE(std::abs(p.compGainReductionDb()-expectedAttack)<.00002);
        const auto releaseCount=frames(rate,120);run(p,0,releaseCount);
        REQUIRE(std::abs(p.compGainReductionDb()-expectedAttack*std::exp(-static_cast<double>(releaseCount)/(.12*rate)))<.00002);
        p.reset();const float compressed=run(p,gain(-12),frames(rate,250));
        REQUIRE(std::abs(20*std::log10(compressed)-(-18))<.0005); // 8dB over /4 -> -18 dBFS
        REQUIRE(std::abs(p.compGainReductionDb()-6)<.0005);
        s.params[CompKnee]=8;p.configure(s,rate);p.reset();run(p,gain(-20),frames(rate,300));
        // Independent soft-knee midpoint: (1 - 1/4) * width / 8 = .75 dB.
        REQUIRE(std::abs(p.compGainReductionDb()-.75)<.0005);
        for(int level:{-30,-24,-20,-16,-12}){
            const double above=level+20.,expected=above<=-4?0:above>=4?above*.75:.75*(above+4)*(above+4)/16;
            p.reset();run(p,gain(level),frames(rate,300));REQUIRE(std::abs(p.compGainReductionDb()-expected)<.0005);
        }
        countAllocations=false;
    }
}
void gateRangeRatioAttackHold(){
    for(double rate:{8000.,44100.,48000.,96000.,192000.}){
        auto s=off();s.params[GateOn]=1;s.params[GateThreshold]=-30;s.params[GateRatio]=3;s.params[GateRange]=30;s.params[GateRelease]=50;s.params[GateAttack]=2;s.params[GateHold]=0;
        Processor p;p.configure(s,rate);countAllocations=true;
        const auto closed=run(p,gain(-50),frames(rate,700));REQUIRE(std::abs(p.gateAttenuationDb()-30)<.0001);
        REQUIRE(std::abs(20*std::log10(closed)+80)<.0002);
        const double initial=p.gateAttenuationDb();const int count=frames(rate,2);run(p,gain(-10),count);
        REQUIRE(std::abs(p.gateAttenuationDb()-initial*std::exp(-static_cast<double>(count)/(.002*rate)))<.00002);
        s.params[GateRatio]=2;s.params[GateRange]=60;p.configure(s,rate);p.reset();run(p,gain(-50),frames(rate,700));REQUIRE(std::abs(p.gateAttenuationDb()-20)<.0001);
        s.params[GateRatio]=1;p.configure(s,rate);p.reset();REQUIRE(run(p,.003f,frames(rate,100))==.003f);REQUIRE(p.gateAttenuationDb()==0);
        s.params[GateRatio]=4;s.params[GateRange]=0;p.configure(s,rate);p.reset();REQUIRE(run(p,.003f,frames(rate,100))==.003f);
        s.params[GateRange]=60;s.params[GateHold]=50;s.params[GateRelease]=5;p.configure(s,rate);p.reset();run(p,gain(-10),frames(rate,10));run(p,gain(-70),frames(rate,30));REQUIRE(p.gateAttenuationDb()==0);
        run(p,gain(-70),frames(rate,60));REQUIRE(p.gateAttenuationDb()>55);
        countAllocations=false;
    }
}
void bypassMixAndFinalLimiter(){
    for(double rate:{8000.,44100.,48000.,96000.,192000.}){
        auto s=off();Processor p;p.configure(s,rate);countAllocations=true;
        for(float input:{-.93f,-.1234567f,0.f,.1234567f,.93f})REQUIRE(p.tick(input)==input);
        s.params[GateOn]=s.params[CompOn]=1;s.params[Wet]=0;s.params[CompMakeup]=24;p.configure(s,rate);
        for(float input:{-.93f,-.1234567f,0.f,.1234567f,.93f})REQUIRE(p.tick(input)==input); // true dry with final limiter OFF
        s.params[LimiterOn]=1;s.params[LimiterCeiling]=-6;s.params[LimiterRelease]=100;s.params[Output]=12;p.configure(s,rate);p.reset();
        const double ceiling=std::pow(10.,-6./20);const auto first=p.tick(1);REQUIRE(std::abs(first-ceiling)<1e-7);REQUIRE(std::abs(p.limiterGainReductionDb()-18)<.0001);
        for(float input:{-10.f,-1.f,.9f,.25f,1.f,10.f})REQUIRE(std::abs(p.tick(input))<=ceiling+1e-7);
        p.reset();p.tick(1);const double firstGain=std::pow(10.,-18./20);const int count=frames(rate,100);run(p,0,count);
        const double expectedGain=1+(firstGain-1)*std::exp(-static_cast<double>(count)/(.1*rate));REQUIRE(std::abs(p.limiterGainReductionDb()+20*std::log10(expectedGain))<.0001);
        // Parallel mix and makeup/output cannot bypass the FINAL limiter.
        for(float wet:{0.f,25.f,75.f,100.f}){s.params[Wet]=wet;p.configure(s,rate);p.reset();for(int i=0;i<500;++i)REQUIRE(std::abs(p.tick(i%2?1.f:-1.f))<=ceiling+1e-7);}
        s.enabled=false;s.params[Output]=12;p.configure(s,rate);REQUIRE(p.tick(.731f)==.731f);REQUIRE(p.gateAttenuationDb()==0 && p.compGainReductionDb()==0 && p.limiterGainReductionDb()==0);
        REQUIRE(p.tick(std::numeric_limits<float>::quiet_NaN())==0);REQUIRE(p.tick(std::numeric_limits<float>::infinity())==0);
        countAllocations=false;
    }
}
void independentInstancesAndLiveChanges(){
    Processor a,b;auto s=off();s.params[CompOn]=1;s.params[CompThreshold]=-30;s.params[CompRatio]=10;a.configure(s,48000);b.configure(off(),48000);
    countAllocations=true;for(int i=0;i<50000;++i){if(i%128==0){s.params[CompThreshold]=-30+static_cast<float>((i/128)%20);a.configure(s,48000);}a.tick(.5f);REQUIRE(b.tick(.25f)==.25f);}
    REQUIRE(a.compGainReductionDb()>0 && b.compGainReductionDb()==0);a.reset();REQUIRE(a.gateAttenuationDb()==0 && a.compGainReductionDb()==0 && a.limiterGainReductionDb()==0);
    s.params.fill(std::numeric_limits<float>::quiet_NaN());a.configure(s,std::numeric_limits<double>::infinity());for(int i=0;i<1000;++i)REQUIRE(std::isfinite(a.tick(i%2?std::numeric_limits<float>::max():-std::numeric_limits<float>::max())));
    countAllocations=false;REQUIRE(allocations==0);
}
void makeupMixAndTrim(){
    auto s=off();s.params[CompOn]=1;s.params[CompThreshold]=-20;s.params[CompRatio]=4;s.params[CompKnee]=0;s.params[CompAttack]=.1f;
    Processor p;p.configure(s,48000);const double compressed=std::pow(10.,-15./20);REQUIRE(std::abs(run(p,1,4800)-compressed)<.00001);
    s.params[CompMakeup]=6;p.configure(s,48000);REQUIRE(std::abs(run(p,1,4800)-compressed*std::pow(10.,6./20))<.00001);
    s.params[Wet]=50;p.configure(s,48000);const double parallel=.5+.5*compressed*std::pow(10.,6./20);REQUIRE(std::abs(p.tick(1)-parallel)<.00001);
    s.params[Output]=-12;p.configure(s,48000);REQUIRE(std::abs(p.tick(1)-parallel*std::pow(10.,-12./20))<.00001);
    s.params[CompOn]=0;s.params[Wet]=100;s.params[Output]=0;p.configure(s,48000);REQUIRE(p.tick(.25f)==.25f);REQUIRE(p.compGainReductionDb()==0);
}
}
int main(){schemaBounds();analyticCurvesAndTimes();gateRangeRatioAttackHold();bypassMixAndFinalLimiter();independentInstancesAndLiveChanges();makeupMixAndTrim();std::cout<<"PASS: Dynamics PRO gate ratio/range/opening/hold/release, compressor soft-knee static curves and attack/release, makeup/parallel mix/output, final sample-peak limiter all wet mixes/output trims, exact dry/bypass, isolated live controls, zero callback allocations, finite guards 8-192k\n";}
