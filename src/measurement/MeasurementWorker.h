#pragma once
// Off-GUI measurement analysis. Only Capture::put() touches the audio callback.
// Worker FFT/GCC-PHAT allocations, waits and result locks NEVER run on the audio thread.
#include "MeasurementCore.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

namespace lah::measurement {
class MeasurementWorker final {
public:
    explicit MeasurementWorker(const Capture& capture) : source(capture), worker([this]{ run(); }) {}
    ~MeasurementWorker() {
        stop.store(true, std::memory_order_release);
        cv.notify_all();
        if (worker.joinable()) worker.join();
    }
    MeasurementWorker(const MeasurementWorker&) = delete;
    MeasurementWorker& operator=(const MeasurementWorker&) = delete;

    void setSampleRate(double sampleRate) noexcept {
        rate.store(std::isfinite(sampleRate) ? sampleRate : 0.0, std::memory_order_release);
    }
    // Call after changing channels/device. Never publish a mixture of old and new capture samples.
    void invalidate(std::uint64_t inFlightFrames=0) noexcept {
        // New data must fill the entire analysis window before result is accepted.
        std::lock_guard<std::mutex> lock(guard);
        minimumCount.store(source.writtenCount() + 32768 + inFlightFrames, std::memory_order_release);
        epoch.fetch_add(1, std::memory_order_acq_rel);
        published={};
        ++generation;
    }
    bool latest(Result& target, std::uint64_t& lastSeen) const {
        std::lock_guard<std::mutex> lock(guard);
        if (generation == lastSeen) return false;
        target = published;
        lastSeen = generation;
        return true;
    }
private:
    void run() {
        std::vector<float> r, m;
        while (!stop.load(std::memory_order_acquire)) {
            const auto before = epoch.load(std::memory_order_acquire);
            const auto fs = rate.load(std::memory_order_acquire);
            Result candidate{};
            if (fs >= 8000.0 && fs <= 192000.0 &&
                source.writtenCount() >= minimumCount.load(std::memory_order_acquire) &&
                source.snapshot(r, m, 32768))
                candidate = analyze(r, m, fs);
            // If input routing changed while the FFT was running, drop this stale result.
            {
                std::lock_guard<std::mutex> lock(guard);
                if (before == epoch.load(std::memory_order_acquire)) {
                    published = std::move(candidate);
                    ++generation;
                }
            }
            std::unique_lock<std::mutex> lock(waitMutex);
            cv.wait_for(lock, std::chrono::milliseconds(350), [this] {
                return stop.load(std::memory_order_acquire);
            });
        }
    }
    const Capture& source;
    mutable std::mutex guard;
    std::mutex waitMutex;
    std::condition_variable cv;
    std::atomic<bool> stop{false};
    std::atomic<double> rate{0};
    std::atomic<std::uint64_t> epoch{0}, minimumCount{32768};
    Result published{};
    std::uint64_t generation=0;
    std::thread worker;
};
} // namespace lah::measurement
