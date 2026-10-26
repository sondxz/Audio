#pragma once
// Non-blocking audio callback safety latch. This does not monitor driver-level XRuns.
// A sustained processing-time overrun is treated as unfit for live output until
// explicitly reset by an operator with the audio muted.
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cmath>

namespace lah::reliability {
class StreamSafety final {
public:
    static constexpr unsigned consecutiveOverrunLimit = 8;
    // Designed for one producer (audio callback), reader(s) on message thread.
    // Returns false when output must be disabled on the NEXT callback.
    bool record(int frameCount,double rate,std::uint64_t durationNs) noexcept {
        if (fault.load(std::memory_order_relaxed)) return false;
        if (frameCount<=0 || frameCount>65536 || !std::isfinite(rate) || rate<8000 || rate>384000) {
            trip(); return false;
        }
        const auto deadline = 1.0e9 * static_cast<double>(frameCount) / rate;
        const bool overrun = static_cast<double>(durationNs) >= deadline;
        auto count = consecutive.load(std::memory_order_relaxed);
        count = overrun ? std::min(count+1u,consecutiveOverrunLimit) : 0u;
        consecutive.store(count,std::memory_order_relaxed);
        if (count>=consecutiveOverrunLimit) {trip();return false;}
        return true;
    }
    bool isFaulted() const noexcept {return fault.load(std::memory_order_acquire);}
    unsigned overrunStreak() const noexcept {return consecutive.load(std::memory_order_relaxed);}
    // Call only with explicit operator acknowledgement AND output muted.
    void clearWhileMuted() noexcept {
        consecutive.store(0,std::memory_order_relaxed);
        fault.store(false,std::memory_order_release);
    }
private:
    void trip() noexcept {fault.store(true,std::memory_order_release);}
    std::atomic<bool> fault{false};
    std::atomic<unsigned> consecutive{0};
};
} // namespace lah::reliability
