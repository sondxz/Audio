#pragma once
#include "../src/measurement/MeasurementCore.h"
#include <memory>

namespace lah::test {
// A Result owns about 136 KiB. Keep fixtures on the heap and isolate the
// return-value temporary so ASan instrumentation/inlining cannot collect many
// independent spectra in one Windows 1 MiB test-thread stack frame.
#if defined(_MSC_VER)
__declspec(noinline)
#elif defined(__GNUC__)
__attribute__((noinline))
#endif
inline std::unique_ptr<measurement::Result> analyzeHeap(const std::vector<float>& reference,
                                                       const std::vector<float>& measured,double rate) {
    return std::make_unique<measurement::Result>(measurement::analyze(reference,measured,rate));
}
} // namespace lah::test
