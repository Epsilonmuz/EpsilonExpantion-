// Epsilon Voice: latency and per-block CPU (share of each block's real-time budget).
//   clang++ -O3 -std=c++17 -ISource Tests/voice_bench.cpp -o voice_bench && ./voice_bench
#include "DSP/EffectEngine.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>
using namespace eps;

int main()
{
    std::printf ("| Rate | Mode | Buffer | Latency | Average | 99th pct | Worst |\n|---|---|---|---|---|---|---|\n");
    const char* names[] = { "Transpose", "Quantize", "Robot" };
    for (double fs : { 44100.0, 48000.0, 96000.0 })
        for (int mode = 0; mode < 3; ++mode)
            for (int block : { 64, 128, 256, 512 })
            {
                if (fs > 50000 && (block == 64 || block == 256)) continue;
                Values v = defaults (Effect::Voice);
                v[voice::mode] = mode; v[voice::pitch] = mode == 2 ? 0 : 5; v[voice::formant] = 2; v[mixSlot] = 1;
                EffectEngine eng (Effect::Voice); eng.prepare (fs, v); eng.setParameters (v, true);
                const int n = int (fs * 10);
                std::vector<float> L ((size_t) block), R ((size_t) block);
                std::vector<double> pct;
                double ph = 0;
                for (int pos = 0; pos + block <= n; pos += block)
                {
                    for (int i = 0; i < block; ++i)
                    {
                        const double t = (pos + i) / fs, f0 = 180 * (1 + 0.01 * std::sin (2 * pi * 5.5 * t)) * std::exp2 (std::floor (t * 2) / 12.0 * 2.0 - 2.0 * std::floor (t / 2.5));
                        ph += f0 / fs; ph -= std::floor (ph);
                        const float s = float (0.3 * (2 * ph - 1) * (0.6 + 0.4 * std::sin (2 * pi * 0.7 * t)));
                        L[size_t (i)] = R[size_t (i)] = s;
                    }
                    const auto t0 = std::chrono::steady_clock::now();
                    eng.process (L.data(), R.data(), block);
                    const double dt = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
                    if (pos > fs) pct.push_back (100 * dt / (block / fs));
                }
                double sum = 0; for (double p : pct) sum += p;
                std::sort (pct.begin(), pct.end());
                std::printf ("| %.1f kHz | %s | %d | %.1f ms | %.1f %% | %.0f %% | %.0f %% |\n", fs / 1000, names[mode], block,
                             1000.0 * eng.latencySamples() / fs, sum / double (pct.size()), pct[size_t (pct.size() * 0.99)], pct.back());
            }
}
