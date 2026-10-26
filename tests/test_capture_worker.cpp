#include "../src/measurement/MeasurementWorker.h"
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <random>
#include <thread>
#include <vector>
#define REQ(x) do{if(!(x)){std::cerr<<"FAILED line "<<__LINE__<<" "<<#x<<"\n";std::exit(1);}}while(false)
int main(){
    using namespace lah::measurement;
    auto captureStorage=std::make_unique<Capture>();
    auto& capture=*captureStorage;
    const int window=8192;
    std::vector<float> ref(window),mic(window);
    std::mt19937 rng(134);std::uniform_real_distribution<float> distribution(-0.2f,0.2f);
    for(int i=0;i<window;++i){ref[i]=distribution(rng);mic[i]=ref[i]*0.5f;}
    std::uint64_t cursor=capture.writtenCount();
    std::vector<float> r,m;
    REQ(!capture.readNext(r,m,window,cursor));
    capture.put(ref.data(),mic.data(),window);
    REQ(capture.readNext(r,m,window,cursor));
    REQ(r.front()==ref.front() && m.back()==mic.back());
    REQ(!capture.readNext(r,m,window,cursor));
    for(int i=0;i<10;++i)capture.put(ref.data(),mic.data(),window);
    REQ(!capture.readNext(r,m,window,cursor)); // gap was detected and cursor re-synced
    REQ(cursor==capture.writtenCount());
    {
        MeasurementWorker worker(capture);
        worker.setSampleRate(48000);
        worker.invalidate();
        for(int i=0;i<4;++i)capture.put(ref.data(),mic.data(),window);
        Result last{};std::uint64_t generation=0;
        bool found=false;
        for(int i=0;i<30;++i){
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if(worker.latest(last,generation) && last.valid){found=true;break;}
        }
        REQ(found && last.meanCoherence>0.95);
        worker.invalidate();
        REQ(worker.latest(last,generation) && !last.valid);
    }
    // A committed sample count cannot expose writes that are still in-flight.
    // Stress wrapping writes; every accepted window must contain one complete
    // generation with coherent REF/MIC pairs, never a partially overwritten mix.
    {
        auto concurrent=std::make_unique<Capture>();
        std::atomic<bool> done{false};
        std::thread producer([&] {
            std::vector<float> a(captureCapacity),b(captureCapacity);
            for(int block=1;block<=80;++block) {
                std::fill(a.begin(),a.end(),static_cast<float>(block)*0.001f);
                std::fill(b.begin(),b.end(),-2.0f*a[0]);
                concurrent->put(a.data(),b.data(),captureCapacity);
            }
            done.store(true,std::memory_order_release);
        });
        int accepted=0;
        do {
            if(concurrent->snapshot(r,m,32768)) {
                ++accepted;
                for(std::size_t i=0;i<r.size();++i) {
                    REQ(r[i]==r[0]);REQ(m[i]==-2.0f*r[i]);
                }
            }
        } while(!done.load(std::memory_order_acquire));
        producer.join();
        REQ(concurrent->snapshot(r,m,32768));
        for(std::size_t i=0;i<r.size();++i)REQ(r[i]==80.0f*0.001f && m[i]==-2.0f*r[i]);
        (void)accepted; // concurrency may legitimately reject every overlapping read
    }
    std::cout<<"PASS: Capture streaming no-overlap, gap detection, worker analyzes OFF audio/UI thread, invalidation\n";
}
