#pragma once

// Actual pre/post samples only. The audio callback is a single producer; one
// worker consumes the bounded queues. Registration/snapshot calls are control
// thread operations. Nodes and their queues live until the owning engine dies.
#include <array>
#include <atomic>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>

namespace lah::spectrum {
constexpr int fftSize=2048,binCount=fftSize/2+1,hopSize=512;
constexpr int maximumSubscriptions=32;
struct Handle { int index=-1;std::uint64_t token=0;explicit operator bool()const noexcept{return index>=0 && token!=0;} };
struct Snapshot {
    std::array<float,binCount> preDb{},postDb{};
    double sampleRate=0;std::uint64_t generation=0;
    bool hasData=false,signalPresent=false;
};
class Capture final {
public:
    explicit Capture(bool startWorker=true):nodes(std::make_unique<Node[]>(maximumSubscriptions)),scratch(std::make_unique<Scratch>()) {
        for(auto& route:routes)route.store(0,std::memory_order_relaxed);
        for(int i=0;i<fftSize;++i){const auto w=.5-.5*std::cos(2*pi*i/(fftSize-1));scratch->window[static_cast<std::size_t>(i)]=static_cast<float>(w);windowSum+=w;}
        if(startWorker)worker=std::thread([this]{while(!stopping.load(std::memory_order_acquire)){processPending();std::this_thread::sleep_for(std::chrono::milliseconds(5));}});
    }
    ~Capture(){stopping.store(true,std::memory_order_release);if(worker.joinable())worker.join();}
    Capture(const Capture&)=delete;Capture& operator=(const Capture&)=delete;
    Handle acquire(bool output,int channel,int slot) {
        if(!valid(channel,slot))return {};
        std::lock_guard<std::mutex> guard(registration);
        const auto target=targetIndex(output,channel,slot);
        const auto route=routes[target].load(std::memory_order_acquire);
        if(route){const int index=routeIndex(route);++nodes[index].references;return {index,route};}
        for(int index=0;index<maximumSubscriptions;++index){auto& node=nodes[index];if(node.references)continue;
            const auto token=(++serial<<8)|static_cast<std::uint64_t>(index+1);
            node.references=1;node.target.store(static_cast<int>(target),std::memory_order_relaxed);
            node.epoch.fetch_add(1,std::memory_order_acq_rel);node.token.store(token,std::memory_order_release);
            routes[target].store(token,std::memory_order_release);return {index,token};
        }
        return {};
    }
    void release(Handle handle)noexcept {
        if(!handle || handle.index>=maximumSubscriptions)return;
        std::lock_guard<std::mutex> guard(registration);auto& node=nodes[handle.index];
        if(node.token.load(std::memory_order_acquire)!=handle.token || !node.references)return;
        if(--node.references==0){routes[static_cast<std::size_t>(node.target.load(std::memory_order_relaxed))].store(0,std::memory_order_release);node.epoch.fetch_add(1,std::memory_order_acq_rel);node.token.store(0,std::memory_order_release);}
    }
    void prepare(double rate)noexcept {
        sampleRate.store(std::isfinite(rate)&&rate>=8000 && rate<=192000?rate:0,std::memory_order_release);
        invalidateDomain(false);invalidateDomain(true);
    }
    void invalidateDomain(bool output)noexcept {
        domainEpoch[output?1:0].fetch_add(1,std::memory_order_acq_rel);
        for(int index=0;index<maximumSubscriptions;++index){auto& node=nodes[index];if(node.target.load(std::memory_order_relaxed)/128==static_cast<int>(output))node.epoch.fetch_add(1,std::memory_order_acq_rel);}
    }
    void invalidateTarget(bool output,int channel,int slot=-1)noexcept {
        if(channel<0 || channel>=32)return;
        for(int s=0;s<4;++s){if(slot>=0 && slot!=s)continue;const auto route=routes[targetIndex(output,channel,s)].load(std::memory_order_acquire);if(route)nodes[routeIndex(route)].epoch.fetch_add(1,std::memory_order_acq_rel);}
    }
    // Call at END of Rack::beginBlock, after its type/reset invalidations. An
    // in-flight callback can never stamp samples with a later PANIC epoch.
    void beginBlock(bool output)noexcept {
        callbackDomain[output?1:0]=domainEpoch[output?1:0].load(std::memory_order_acquire);
        for(int index=0;index<maximumSubscriptions;++index){auto& node=nodes[index];if(node.target.load(std::memory_order_relaxed)/128!=static_cast<int>(output))continue;
            callbackEpoch[static_cast<std::size_t>(index)]=node.epoch.load(std::memory_order_acquire);callbackToken[static_cast<std::size_t>(index)]=node.token.load(std::memory_order_acquire);
        }
    }
    // Called only by the one audio callback thread. SPSC release/acquire makes
    // ordinary Frame storage race-free; a full queue drops new samples.
    void push(bool output,int channel,int slot,float pre,float post)noexcept {
        if(!valid(channel,slot))return;
        const auto route=routes[targetIndex(output,channel,slot)].load(std::memory_order_acquire);if(!route)return;
        const auto index=static_cast<std::size_t>(routeIndex(route));auto& node=nodes[index];
        const auto domain=callbackDomain[output?1:0],epoch=callbackEpoch[index];
        if(callbackToken[index]!=route || node.token.load(std::memory_order_acquire)!=route || domain!=domainEpoch[output?1:0].load(std::memory_order_acquire) || epoch!=node.epoch.load(std::memory_order_acquire))return;
        const auto write=node.write.load(std::memory_order_relaxed);
        if(write-node.read.load(std::memory_order_acquire)>=queueSize){node.dropped.fetch_add(1,std::memory_order_release);return;}
        node.queue[static_cast<std::size_t>(write%queueSize)]={std::isfinite(pre)?pre:0,std::isfinite(post)?post:0,route,epoch,domain};
        node.write.store(write+1,std::memory_order_release);
        // Updating a timestamp once per 128 samples bounds callback overhead.
        if((write&127u)==0)node.lastCapture.store(nowNs(),std::memory_order_release);
    }
    bool read(Handle handle,Snapshot& result)const noexcept {
        result.hasData=false;result.signalPresent=false;result.generation=0;
        if(!handle || handle.index>=maximumSubscriptions)return false;
        const auto& node=nodes[handle.index];
        const auto epoch=node.epoch.load(std::memory_order_acquire),token=node.token.load(std::memory_order_acquire);
        const auto domainIndex=node.target.load(std::memory_order_relaxed)/128;
        const auto domain=domainEpoch[domainIndex].load(std::memory_order_acquire);
        if(token!=handle.token)return false;
        const auto stamp=node.lastCapture.load(std::memory_order_acquire);
        if(stamp==0 || nowNs()-stamp>500000000)return false;
        const auto sequence=node.publication.load(std::memory_order_acquire);
        if((sequence&1) || !sequence || node.publishedToken.load(std::memory_order_relaxed)!=token || node.publishedEpoch.load(std::memory_order_relaxed)!=epoch || node.publishedDomain.load(std::memory_order_relaxed)!=domain || node.publishedDropped.load(std::memory_order_relaxed)!=node.dropped.load(std::memory_order_acquire))return false;
        for(std::size_t bin=0;bin<binCount;++bin){result.preDb[bin]=node.preDb[bin].load(std::memory_order_relaxed);result.postDb[bin]=node.postDb[bin].load(std::memory_order_relaxed);}
        result.sampleRate=node.publishedRate.load(std::memory_order_relaxed);result.signalPresent=node.signal.load(std::memory_order_relaxed);
        if(sequence!=node.publication.load(std::memory_order_acquire) || token!=node.token.load(std::memory_order_acquire) || epoch!=node.epoch.load(std::memory_order_acquire) || domain!=domainEpoch[domainIndex].load(std::memory_order_acquire))return false;
        result.generation=sequence/2;result.hasData=true;return true;
    }
    // Worker only. Public for deterministic offline tests; never call while the
    // built-in worker is enabled. FFT/history/scratch are preallocated on heap.
    void processPending()noexcept {
        for(int index=0;index<maximumSubscriptions;++index){auto& node=nodes[index];
            auto readIndex=node.read.load(std::memory_order_relaxed);const auto write=node.write.load(std::memory_order_acquire);
            const auto currentToken=node.token.load(std::memory_order_acquire),currentEpoch=node.epoch.load(std::memory_order_acquire),dropped=node.dropped.load(std::memory_order_acquire);
            const auto currentDomain=domainEpoch[node.target.load(std::memory_order_relaxed)>=128?1:0].load(std::memory_order_acquire);
            if(dropped!=node.workerDropped){node.count=node.position=node.sinceFft=0;node.workerDropped=dropped;node.read.store(write,std::memory_order_release);continue;}
            if(currentToken!=node.workerToken || currentEpoch!=node.workerEpoch || currentDomain!=node.workerDomain || dropped!=node.workerDropped){node.count=node.position=node.sinceFft=0;node.workerToken=currentToken;node.workerEpoch=currentEpoch;node.workerDomain=currentDomain;node.workerDropped=dropped;}
            for(;readIndex<write;++readIndex){const auto frame=node.queue[static_cast<std::size_t>(readIndex%queueSize)];
                if(!currentToken || frame.token!=currentToken || frame.epoch!=currentEpoch || frame.domain!=currentDomain)continue;
                node.pre[static_cast<std::size_t>(node.position)]=frame.pre;node.post[static_cast<std::size_t>(node.position)]=frame.post;
                node.position=(node.position+1)%fftSize;node.count=std::min(fftSize,node.count+1);++node.sinceFft;
                if(node.count==fftSize && node.sinceFft>=hopSize){node.sinceFft=0;publish(node,currentToken,currentEpoch,currentDomain,dropped);}
            }
            node.read.store(readIndex,std::memory_order_release);
        }
    }
private:
    static constexpr double pi=3.14159265358979323846;
    static constexpr std::uint64_t queueSize=8192;
    struct Frame {float pre=0,post=0;std::uint64_t token=0,epoch=0,domain=0;};
    struct Node {
        int references=0; // registration mutex only
        std::atomic<int> target{-1};std::atomic<std::uint64_t> token{0},epoch{1},write{0},read{0},dropped{0},lastCapture{0};
        std::array<Frame,queueSize> queue{};
        std::array<float,fftSize> pre{},post{};
        int count=0,position=0,sinceFft=0;std::uint64_t workerToken=0,workerEpoch=0,workerDomain=0,workerDropped=0;
        std::atomic<std::uint64_t> publication{0},publishedToken{0},publishedEpoch{0},publishedDomain{0},publishedDropped{0};
        std::atomic<double> publishedRate{0};std::atomic<bool> signal{false};
        std::array<std::atomic<float>,binCount> preDb{},postDb{};
    };
    struct Scratch {std::array<std::complex<double>,fftSize> fft{};std::array<float,fftSize> window{};};
    static bool valid(int channel,int slot)noexcept{return channel>=0 && channel<32 && slot>=0 && slot<4;}
    static std::size_t targetIndex(bool output,int channel,int slot)noexcept{return static_cast<std::size_t>((output?128:0)+channel*4+slot);}
    static int routeIndex(std::uint64_t route)noexcept{return static_cast<int>((route&255)-1);}
    static std::uint64_t nowNs()noexcept{return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
    void transform()noexcept {
        auto& data=scratch->fft;
        for(int i=1,j=0;i<fftSize;++i){int bit=fftSize>>1;for(;j&bit;bit>>=1)j^=bit;j^=bit;if(i<j)std::swap(data[static_cast<std::size_t>(i)],data[static_cast<std::size_t>(j)]);}
        for(int length=2;length<=fftSize;length*=2){const auto step=std::polar(1.,-2*pi/length);
            for(int begin=0;begin<fftSize;begin+=length){std::complex<double> w{1,0};for(int j=0;j<length/2;++j){const auto a=data[static_cast<std::size_t>(begin+j)],b=data[static_cast<std::size_t>(begin+j+length/2)]*w;data[static_cast<std::size_t>(begin+j)]=a+b;data[static_cast<std::size_t>(begin+j+length/2)]=a-b;w*=step;}}
        }
    }
    void publish(Node& node,std::uint64_t token,std::uint64_t epoch,std::uint64_t domain,std::uint64_t dropped)noexcept {
        const auto rate=sampleRate.load(std::memory_order_acquire);if(rate<8000)return;
        node.publication.fetch_add(1,std::memory_order_acq_rel);bool signal=false;
        for(int side=0;side<2;++side){const auto& source=side?node.post:node.pre;
            for(int sample=0;sample<fftSize;++sample){const auto value=source[static_cast<std::size_t>((node.position+sample)%fftSize)];signal=signal || std::abs(value)>1e-9f;scratch->fft[static_cast<std::size_t>(sample)]={value*scratch->window[static_cast<std::size_t>(sample)],0};}
            transform();auto& destination=side?node.postDb:node.preDb;
            for(int bin=0;bin<binCount;++bin){const double scale=bin==0 || bin==fftSize/2?1:2;const auto amplitude=std::abs(scratch->fft[static_cast<std::size_t>(bin)])*scale/windowSum;destination[static_cast<std::size_t>(bin)].store(static_cast<float>(20*std::log10(std::max(1e-7,amplitude))),std::memory_order_relaxed);}
        }
        node.publishedToken.store(token,std::memory_order_relaxed);node.publishedEpoch.store(epoch,std::memory_order_relaxed);node.publishedDomain.store(domain,std::memory_order_relaxed);node.publishedDropped.store(dropped,std::memory_order_relaxed);
        node.publishedRate.store(rate,std::memory_order_relaxed);node.signal.store(signal,std::memory_order_relaxed);node.publication.fetch_add(1,std::memory_order_release);
    }
    std::unique_ptr<Node[]> nodes;std::unique_ptr<Scratch> scratch;
    std::array<std::atomic<std::uint64_t>,256> routes{};
    std::array<std::atomic<std::uint64_t>,2> domainEpoch{{1,1}};
    std::array<std::uint64_t,2> callbackDomain{};
    std::array<std::uint64_t,maximumSubscriptions> callbackEpoch{},callbackToken{};
    std::atomic<double> sampleRate{48000};double windowSum=0;
    std::mutex registration;std::uint64_t serial=0;
    std::atomic<bool> stopping{false};std::thread worker;
};
}
