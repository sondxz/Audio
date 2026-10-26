#pragma once
#include "FeedbackModelWeights.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace lah::feedback {
constexpr double pi=3.14159265358979323846;
constexpr int fftSize=1024,maxNotches=6;
struct Settings {
    bool armed=false,useModel=false;
    float persistenceMs=800,modelThreshold=.9f,maxDepthDb=12,q=30;
    std::uint32_t resetGeneration=0;
};
struct Status {
    float candidateHz=0,modelScore=0;
    int notchCount=0;
    std::array<float,maxNotches> notchHz{},depthDb{};
    bool armed=false;
};
class Processor {
    struct Filter {
        double b0=1,b1=0,b2=0,a1=0,a2=0,z1=0,z2=0;
        float tick(float x)noexcept {
            const double y=b0*x+z1;z1=b1*x-a1*y+z2;z2=b2*x-a2*y;
            if(!std::isfinite(y)||!std::isfinite(z1)||!std::isfinite(z2)){clear();return 0;}
            if(std::abs(z1)<1e-25)z1=0;if(std::abs(z2)<1e-25)z2=0;
            return static_cast<float>(y);
        }
        void clear()noexcept {z1=z2=0;}
        void lowpass(double hz,double q,double fs)noexcept {
            const double w=2*pi*hz/fs,c=std::cos(w),a=std::sin(w)/(2*q),d=1+a;
            b0=(1-c)*.5/d;b1=(1-c)/d;b2=b0;a1=-2*c/d;a2=(1-a)/d;
        }
        void bell(double hz,double q,double db,double fs)noexcept {
            const double w=2*pi*hz/fs,c=std::cos(w),a=std::sin(w)/(2*q),A=std::pow(10.,db/40),d=1+a/A;
            b0=(1+a*A)/d;b1=-2*c/d;b2=(1-a*A)/d;a1=b1;a2=(1-a/A)/d;
        }
    };
    struct Notch {Filter filter;float hz=0,depth=0,mix=0;};
    struct Track {
        float hz=0,lastDb=-120,growthDb=0;
        int age=0,missing=0,growing=0;
        bool confirmed=false,seen=false;
    };
public:
    void prepare(double sampleRate,int phaseOffset=0) {
        rate=std::isfinite(sampleRate)?std::clamp(sampleRate,8000.,192000.):48000.;
        decimation=std::max(1,static_cast<int>(std::ceil(rate/24000.)));
        analysisRate=rate/decimation;phase=phaseOffset%256;
        for(int i=0;i<fftSize;++i)window[static_cast<std::size_t>(i)]=static_cast<float>(.5-.5*std::cos(2*pi*i/(fftSize-1)));
        for(int i=0;i<fftSize/2;++i){cosine[static_cast<std::size_t>(i)]=static_cast<float>(std::cos(-2*pi*i/fftSize));sine[static_cast<std::size_t>(i)]=static_cast<float>(std::sin(-2*pi*i/fftSize));}
        antiAlias[0].lowpass(analysisRate*.4,.541196100146197,rate);
        antiAlias[1].lowpass(analysisRate*.4,1.30656296487638,rate);
        model.prepare(analysisRate);reset();
    }
    void reset()noexcept {
        // filled=0 makes old ring samples unreachable until all have been overwritten.
        tracks={};notches={};for(auto& f:antiAlias)f.clear();
        position=filled=counter=0;hop=phase;state={};settings.armed=false;
        model.reset();clearRamp=0;clearDelta=lastDelta=0;++serial;
    }
    void configure(Settings next)noexcept {
        next.persistenceMs=std::isfinite(next.persistenceMs)?std::clamp(next.persistenceMs,100.f,3000.f):800.f;
        next.maxDepthDb=std::isfinite(next.maxDepthDb)?std::clamp(next.maxDepthDb,3.f,12.f):12.f;
        next.q=std::isfinite(next.q)?std::clamp(next.q,10.f,60.f):30.f;
        next.modelThreshold=std::isfinite(next.modelThreshold)?std::clamp(next.modelThreshold,.5f,.999f):.9f;
        if(next.resetGeneration!=settings.resetGeneration || next.useModel!=settings.useModel){const float delta=lastDelta;reset();clearDelta=delta;clearRamp=64;}
        if(next.armed!=settings.armed){tracks={};++serial;}
        settings=next;state.armed=next.armed;
    }
    float processSample(float input)noexcept {
        const float dry=std::isfinite(input)?std::clamp(input,-16.f,16.f):0;
        float y=dry;
        for(auto& n:notches)if(n.depth>0){
            const float wet=n.filter.tick(y);n.mix=std::min(1.f,n.mix+1.f/64.f);y+=(wet-y)*n.mix;
        }
        if(clearRamp>0){y+=clearDelta*static_cast<float>(--clearRamp)/64.f;}
        lastDelta=y-dry;
        // Analyze the residual so a suppressed tone does not hide a second resonance.
        float analysis=antiAlias[1].tick(antiAlias[0].tick(y));
        if(++counter>=decimation){
            counter=0;if(settings.useModel)model.processSample(analysis);
            ring[static_cast<std::size_t>(position)]=analysis;position=(position+1)%fftSize;
            filled=std::min(fftSize,filled+1);
            if(++hop>=256){hop=0;if(filled==fftSize)analyze();}
        }
        return std::isfinite(y)?y:0;
    }
    Status status()const noexcept{return state;}
    std::uint32_t revision()const noexcept{return serial;}
private:
    void transform()noexcept {
        for(int i=0;i<fftSize;++i){re[static_cast<std::size_t>(i)]=ring[static_cast<std::size_t>((position+i)%fftSize)]*window[static_cast<std::size_t>(i)];im[static_cast<std::size_t>(i)]=0;}
        for(int i=1,j=0;i<fftSize;++i){int bit=fftSize>>1;for(;j&bit;bit>>=1)j^=bit;j^=bit;if(i<j){std::swap(re[static_cast<std::size_t>(i)],re[static_cast<std::size_t>(j)]);}}
        for(int length=2;length<=fftSize;length*=2){
            const int half=length/2,step=fftSize/length;
            for(int base=0;base<fftSize;base+=length)for(int j=0;j<half;++j){
                const auto a=static_cast<std::size_t>(base+j),b=static_cast<std::size_t>(base+j+half),t=static_cast<std::size_t>(j*step);
                const float r=cosine[t]*re[b]-sine[t]*im[b],v=sine[t]*re[b]+cosine[t]*im[b];
                re[b]=re[a]-r;im[b]=im[a]-v;re[a]+=r;im[a]+=v;
            }
        }
        for(int k=0;k<fftSize/2;++k)power[static_cast<std::size_t>(k)]=re[static_cast<std::size_t>(k)]*re[static_cast<std::size_t>(k)]+im[static_cast<std::size_t>(k)]*im[static_cast<std::size_t>(k)];
    }
    void analyze()noexcept {
        transform();++serial;state.modelScore=settings.useModel?std::clamp(model.score(),0.f,1.f):0;state.candidateHz=0;
        std::array<bool,fftSize/2> selected{};
        for(auto& t:tracks){t.seen=false;++t.missing;}
        double total=0;for(int k=1;k<fftSize/2;++k)total+=power[static_cast<std::size_t>(k)];
        const int lo=std::max(3,static_cast<int>(std::ceil(80*fftSize/analysisRate)));
        const int hi=std::min(fftSize/2-10,static_cast<int>(std::floor(std::min(8000.,analysisRate*.38)*fftSize/analysisRate)));
        for(int pick=0;pick<4;++pick){
            int bin=-1;float best=0;
            for(int k=lo;k<=hi;++k)if(!selected[static_cast<std::size_t>(k)] && power[static_cast<std::size_t>(k)]>best && power[static_cast<std::size_t>(k)]>power[static_cast<std::size_t>(k-1)] && power[static_cast<std::size_t>(k)]>=power[static_cast<std::size_t>(k+1)]){bin=k;best=power[static_cast<std::size_t>(k)];}
            if(bin<0)break;
            const float center=power[static_cast<std::size_t>(bin)],left=power[static_cast<std::size_t>(bin-1)],right=power[static_cast<std::size_t>(bin+1)];
            double local=0;int neighbors=0;
            for(int k=std::max(1,bin-9);k<=std::min(fftSize/2-1,bin+9);++k)if(std::abs(k-bin)>=4){local+=power[static_cast<std::size_t>(k)];++neighbors;}
            const float concentration=static_cast<float>((left+center+right)/std::max(total,1e-20));
            const float prominence=static_cast<float>(10*std::log10(center/std::max(local/std::max(1,neighbors),1e-20)));
            const float a=std::log(std::max(left,1e-20f)),b=std::log(std::max(center,1e-20f)),c=std::log(std::max(right,1e-20f));
            const float denominator=a-2*b+c,offset=std::abs(denominator)>1e-12f?std::clamp(.5f*(a-c)/denominator,-.5f,.5f):0;
            const float hz=static_cast<float>((bin+offset)*analysisRate/fftSize);
            const float db=10*std::log10(std::max(left+center+right,1e-20f))-20*std::log10(static_cast<float>(fftSize)/2);
            float harmonic=0;
            for(int multiple=2;multiple<=3;++multiple){const int h=static_cast<int>(std::lround((bin+offset)*multiple));if(h>1&&h<fftSize/2-1)harmonic+=power[static_cast<std::size_t>(h-1)]+power[static_cast<std::size_t>(h)]+power[static_cast<std::size_t>(h+1)];}
            const bool eligible=concentration>.16f&&prominence>15&&db>-65&&harmonic/(left+center+right+1e-20f)<.3f;
            if(eligible){
                if(state.candidateHz==0)state.candidateHz=hz;
                Track* found=nullptr;
                for(auto& t:tracks)if(t.hz>0&&std::abs(t.hz-hz)<std::max(8.f,hz*.012f)){found=&t;break;}
                if(!found)for(auto& t:tracks)if(t.hz==0||t.missing>6){t={};t.hz=hz;t.lastDb=db;found=&t;break;}
                if(found){auto& t=*found;const float growth=db-t.lastDb;
                    t.seen=true;t.missing=0;++t.age;t.hz=.8f*t.hz+.2f*hz;
                    if(growth>.08f&&growth<6){++t.growing;t.growthDb+=growth;}
                    else if(growth<-.5f){t.growing=0;t.growthDb=0;}
                    if(t.growing>=4&&t.growthDb>=6)t.confirmed=true;
                    t.lastDb=db;
                    const float elapsed=static_cast<float>(t.age*256000./analysisRate);
                    if(settings.armed&&t.confirmed&&elapsed>=settings.persistenceMs&&(!settings.useModel||state.modelScore>=settings.modelThreshold))install(t.hz);
                }
            }
            for(int k=std::max(1,bin-3);k<=std::min(fftSize/2-1,bin+3);++k)selected[static_cast<std::size_t>(k)]=true;
        }
        for(auto& t:tracks)if(t.missing>6)t={};
    }
    void install(float hz)noexcept {
        for(const auto& n:notches)if(n.depth>0&&std::abs(n.hz-hz)<std::max(15.f,hz*.035f))return;
        float used=0;for(const auto& n:notches)used+=n.depth;
        const float depth=std::min(settings.maxDepthDb,36.f-used);if(depth<3)return;
        for(int i=0;i<maxNotches;++i)if(notches[static_cast<std::size_t>(i)].depth==0){
            auto& n=notches[static_cast<std::size_t>(i)];n.hz=hz;n.depth=depth;n.mix=0;n.filter.clear();n.filter.bell(hz,settings.q,-depth,rate);
            state.notchHz[static_cast<std::size_t>(i)]=hz;state.depthDb[static_cast<std::size_t>(i)]=-depth;++state.notchCount;return;
        }
    }
    double rate=48000,analysisRate=24000;
    int decimation=2,phase=0,position=0,filled=0,counter=0,hop=0;
    int clearRamp=0;float clearDelta=0,lastDelta=0;
    std::uint32_t serial=0;
    Settings settings;Status state;
    std::array<Filter,2> antiAlias;
    std::array<Notch,maxNotches> notches;
    std::array<Track,8> tracks;
    std::array<float,fftSize> ring{},window{},re{},im{};
    std::array<float,fftSize/2> cosine{},sine{},power{};
    feedback_model::Analyzer model;
};
}
