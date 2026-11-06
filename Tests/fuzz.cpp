// Random automation fuzz on all nine engines (pluginval-style): random values every 32 samples,
// extremes often, noise + silence + loud input, random block sizes, several sample rates.
#include "DSP/EffectEngine.h"
#include <cstdio>
#include <random>
#include <vector>
#include <memory>
using namespace eps;
int main()
{
    std::mt19937 rng (7);
    auto u = [&] { return std::uniform_real_distribution<double> (0, 1) (rng); };
    const char* names[] = { "HEAT", "VALVE", "CRUSH", "PRISM", "TIME", "PLATE", "SPACE", "TONE", "VOICE" };
    int failures = 0;
    for (int e = 0; e < 9; ++e)
    {
        long bad = 0; double maxAbs = 0;
        for (double fs : { 44100.0, 48000.0, 96000.0 })
        {
            auto eng = std::make_unique<EffectEngine> (Effect (e));
            auto d = descriptor (Effect (e));
            Values v = defaults (Effect (e));
            eng->prepare (fs, v);
            std::vector<float> L (4096), R (4096);
            const int total = (int) (fs * 4);
            for (int done = 0; done < total;)
            {
                int n = 1 + (int) (u() * 1024);
                for (int k = 0; k < 3; ++k)
                {
                    size_t i = (size_t) (rng() % slots);
                    if (! d.spec[i].id || i == bypassSlot) continue;
                    double t = u(); t = t < 0.15 ? 0 : t > 0.85 ? 1 : u();
                    v[i] = d.spec[i].lo + t * (d.spec[i].hi - d.spec[i].lo);
                }
                v[5] = std::min (v[5], 0.0);                       // keep output trim sane for the level check
                eng->setParameters (v);
                const int mode = (int) (u() * 3);
                for (int i = 0; i < n; ++i)
                {
                    double x = mode == 0 ? 0.0 : (u() * 2 - 1) * (mode == 1 ? 0.25 : 1.0);
                    L[(size_t) i] = (float) x; R[(size_t) i] = (float) (u() * 2 - 1) * (mode == 0 ? 0.f : 0.25f);
                }
                Transport t; t.hostTempo = u() < 0.5; t.bpm = 60 + 120 * u();
                eng->process (L.data(), R.data(), n, t);
                for (int i = 0; i < n; ++i)
                {
                    if (! std::isfinite (L[(size_t) i]) || ! std::isfinite (R[(size_t) i])) ++bad;
                    else maxAbs = std::max (maxAbs, (double) std::max (std::abs (L[(size_t) i]), std::abs (R[(size_t) i])));
                }
                done += n;
            }
        }
        printf ("%-6s non-finite samples: %ld   peak output: %.1f dBFS\n", names[e], bad, 20 * std::log10 (maxAbs + 1e-12));
        failures += bad > 0;
    }
    return failures;
}
