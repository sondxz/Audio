#pragma once
// Independent SPL history worker. Reads non-overlapping capture windows and
// refuses to integrate across a capture gap. Never locks audio callback.
#include "SplStatisticsCore.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

namespace lah::spl {
class SplWorker final {
public:
    explicit SplWorker(const measurement::Capture& capture):source(capture),worker([this]{run();}) {}
    ~SplWorker(){stop.store(true);wake.notify_all();if(worker.joinable())worker.join();}
    SplWorker(const SplWorker&)=delete;
    SplWorker& operator=(const SplWorker&)=delete;
    void configure(const Calibration& c){
        std::lock_guard<std::mutex> lock(guard);
        calibration=c;statistics.clear();cursor=source.writtenCount();last={};history={};gap=false;
        ++generation;
    }
    void reset(){configure({});}
    void snapshot(Reading& reading,Statistics& stats,bool& missingSamples) const {
        std::lock_guard<std::mutex> lock(guard);
        reading=last;stats=history;missingSamples=gap;
    }
private:
    void run(){
        std::vector<float> r,m;
        while(!stop.load(std::memory_order_acquire)){
            // Runs only in this worker, not UI or audio callback. Work in bounded batches.
            for(int i=0;i<12;++i){
                std::lock_guard<std::mutex> lock(guard);
                if(!calibration.valid)break;
                const auto currentCount=source.writtenCount();
                if(cursor>currentCount || currentCount-cursor>measurement::captureCapacity-8192) {
                    gap=true;statistics.clear();cursor=currentCount;
                    last={};history={};++generation;break;
                }
                if(!source.readNext(r,m,8192,cursor))break;
                const auto current=analyze(m,calibration.rate,calibration);
                if(!current.valid){
                    gap=true;statistics.clear();history={};last=current;
                } else {
                    last=current;
                    if(statistics.add(current))history=statistics.current();
                }
                ++generation;
            }
            std::unique_lock<std::mutex> lock(waitGuard);
            wake.wait_for(lock,std::chrono::milliseconds(100),[this]{return stop.load();});
        }
    }
    const measurement::Capture& source;
    mutable std::mutex guard;
    std::mutex waitGuard;
    std::condition_variable wake;
    std::atomic<bool> stop{false};
    Calibration calibration;
    SoundLevelStatistics statistics;
    Reading last;
    Statistics history;
    std::uint64_t cursor=0,generation=0;
    bool gap=false;
    std::thread worker;
};
} // namespace lah::spl
