#include "DSP/Primitives.h"
#include <cstdio>
#include <complex>
using namespace eps;
struct Id { double operator() (double u) const { return u; } };
int main()
{
    for (int st = 1; st <= 3; ++st)
    {
        Oversampler<Id> os; os.prepare(); Id id;
        // phase delay at several frequencies from a steady sine
        printf ("stages %d (latency() = %d):", st, Oversampler<Id>::latency (st));
        for (double f : { 100.0, 1000.0, 8000.0, 16000.0 })
        {
            os.reset(); const double fs = 48000; std::complex<double> acc = 0, ref = 0;
            for (int n = 0; n < 48000; ++n)
            {
                const double x = std::sin (2 * pi * f * n / fs), y = os.tick (x, st, id);
                if (n > 24000) { std::complex<double> e = std::polar (1.0, -2 * pi * f * n / fs); acc += y * e; ref += x * e; }
            }
            const double ph = std::arg (acc / ref), mag = std::abs (acc / ref);
            double d = -ph / (2 * pi * f / fs); while (d < 0) d += fs / f;
            printf ("  %g Hz: delay %.3f smp, gain %+.3f dB", f, d, 20 * std::log10 (mag));
        }
        printf ("\n");
    }
}
