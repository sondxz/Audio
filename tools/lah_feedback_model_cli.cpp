// Offline ML feature bridge; never opens an audio device.
#include "../src/dsp/FeedbackModelWeights.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

namespace {
int selfTest() {
    using namespace lah::feedback_model;
    std::array<float, featureCount> damaged {};
    damaged[0] = std::numeric_limits<float>::quiet_NaN();
    if (classify(damaged) != 0) return 10;
    damaged[0] = std::numeric_limits<float>::infinity();
    if (classify(damaged) != 0) return 11;
    Analyzer a, b;
    a.prepare(16000); b.prepare(16000);
    for (int n = 0; n < 1600; ++n) {
        const bool ready = a.processSample(std::numeric_limits<float>::quiet_NaN());
        if (b.processSample(0) != ready) return 12;
        if (ready && (a.features() != b.features() || a.score() != b.score())) return 13;
    }
    a.reset();
    if (a.score() != 0 || a.estimatedToneHz() != 0) return 14;
    for (float x : a.features()) if (x != 0) return 15;
    for (int n = 0; n < 1600; ++n) {
        if (a.processSample(n % 2 ? std::numeric_limits<float>::max() : -std::numeric_limits<float>::max())) {
            if (!std::isfinite(a.score()) || !std::isfinite(a.estimatedToneHz())) return 16;
            for (float x : a.features()) if (!std::isfinite(x)) return 17;
        }
    }
    return 0;
}
}

int main(int argc, char** argv) {
    if (argc == 2 && std::strcmp(argv[1], "--self-test") == 0) return selfTest();
    if (argc != 2) return 2;
    char* end = nullptr;
    const double rate = std::strtod(argv[1], &end);
    if (!end || *end || !std::isfinite(rate) || rate < 8000 || rate > 192000) return 3;
#ifdef _WIN32
    if (_setmode(_fileno(stdin), _O_BINARY) == -1 || _setmode(_fileno(stdout), _O_BINARY) == -1) return 4;
#endif
    lah::feedback_model::Analyzer analyzer;
    analyzer.prepare(rate);
    std::array<float, 1024> input {};
    for (;;) {
        const auto count = std::fread(input.data(), sizeof(float), input.size(), stdin);
        for (std::size_t i = 0; i < count; ++i) {
            if (!analyzer.processSample(input[i])) continue;
            std::array<float, 14> row {};
            const auto& f = analyzer.features();
            std::copy(f.begin(), f.end(), row.begin());
            row[12] = analyzer.score(); row[13] = analyzer.estimatedToneHz();
            if (std::fwrite(row.data(), sizeof(float), row.size(), stdout) != row.size()) return 5;
        }
        if (count < input.size()) break;
    }
    return std::ferror(stdin) || std::ferror(stdout) ? 6 : 0;
}
