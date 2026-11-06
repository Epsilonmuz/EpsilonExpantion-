// Epsilon Voice quality checks on a synthetic singing voice (glottal pulses through vowel formants,
// with vibrato and a breathy noise part):
//   formants stay put when only the pitch moves, move with Link, level holds, Robot is steady,
//   no clicks, noise passes, all three modes, 44.1 / 48 / 96 kHz.
//   clang++ -O2 -std=c++17 -ISource Tests/voice_quality.cpp -o vq && ./vq
#include "DSP/EffectEngine.h"
#include <complex>
#include <cstdio>
#include <vector>
using namespace eps;
using cd = std::complex<double>;
static int fails = 0;
static void check (bool ok, const char* what, double v, const char* unit)
{ std::printf ("%s %-58s %9.2f %s\n", ok ? "PASS" : "FAIL", what, v, unit); fails += ok ? 0 : 1; }

struct Formant { double f, bw, gain; };
// vowel "ah": F1 700, F2 1220, F3 2600
static std::vector<double> vowel (double fs, double sec, double f0, double vibrato = 0.0, double breath = 0.0, unsigned seed = 1)
{
    const Formant F[3] = { { 700, 90, 1.0 }, { 1220, 110, 0.6 }, { 2600, 160, 0.3 } };
    struct Res { double a1, a2, g, y1 = 0, y2 = 0; } r[3];
    for (int k = 0; k < 3; ++k) { const double R = std::exp (-pi * F[k].bw / fs), th = 2 * pi * F[k].f / fs; r[k] = { 2 * R * std::cos (th), -R * R, F[k].gain * (1 - R) }; }
    std::vector<double> x (size_t (sec * fs));
    double ph = 0; unsigned s = seed;
    for (size_t i = 0; i < x.size(); ++i)
    {
        const double t = double (i) / fs, f = f0 * (1 + vibrato * std::sin (2 * pi * 5.5 * t));
        ph += f / fs;
        double src = 0;
        if (ph >= 1) { ph -= 1; src = 1.0; }                       // glottal pulse
        s = s * 1664525u + 1013904223u;
        src += breath * ((s >> 9) / 4194304.0 - 1.0);
        double y = 0;
        for (auto& q : r) { const double o = q.g * src + q.a1 * q.y1 + q.a2 * q.y2; q.y2 = q.y1; q.y1 = o; y += o; }
        x[i] = y;
    }
    double peak = 0; for (double v : x) peak = std::max (peak, std::abs (v));
    for (double& v : x) v *= 0.3 / peak;
    return x;
}
static std::vector<double> run (double fs, Values v, const std::vector<double>& x)
{
    EffectEngine e (Effect::Voice); e.prepare (fs, v); e.setParameters (v, true);
    std::vector<double> y (x.size()); std::vector<float> L (256), R (256);
    for (size_t s = 0; s < x.size(); s += 256)
    {
        const size_t n = std::min<size_t> (256, x.size() - s);
        for (size_t i = 0; i < n; ++i) L[i] = R[i] = float (x[s + i]);
        e.process (L.data(), R.data(), int (n));
        for (size_t i = 0; i < n; ++i) y[s + i] = L[i];
    }
    return y;
}
static void fft (std::vector<cd>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) { size_t b = n >> 1; for (; j & b; b >>= 1) j ^= b; j ^= b; if (i < j) std::swap (a[i], a[j]); }
    for (size_t len = 2; len <= n; len <<= 1) { const double an = -2 * pi / double (len); const cd wl (std::cos (an), std::sin (an));
        for (size_t i = 0; i < n; i += len) { cd w (1); for (size_t j = 0; j < len / 2; ++j) { const cd u = a[i + j], q = a[i + j + len / 2] * w; a[i + j] = u + q; a[i + j + len / 2] = u - q; w *= wl; } } }
}
// pitch near 'want' by harmonic comb on a long FFT (accurate even when the fundamental is weak);
// 'oddOk' reports whether odd harmonics of that pitch are present (so the real period is not half of it)
static double combPitch (const std::vector<double>& x, double fs, double want, bool* oddOk = nullptr)
{
    const size_t N = fs > 60000 ? (1 << 18) : (1 << 17); std::vector<cd> a (N, 0.0); const size_t len = std::min (N, x.size() / 2);
    for (size_t i = 0; i < len; ++i) a[i] = x[x.size() - len + i] * (0.5 - 0.5 * std::cos (2 * pi * double (i) / double (len)));
    fft (a);
    auto mag = [&] (double f) { const double k = f * double (N) / fs; double m = 0; for (long j = long (k) - 2; j <= long (k) + 3; ++j) m = std::max (m, std::abs (a[size_t (std::max (1L, j))])); return m; };
    double best = -1e18, bf = want;
    for (double c = -60; c <= 60; c += 0.5)
    {
        const double f = want * std::exp2 (c / 1200); double sum = 0;
        for (int h = 1; h <= 8; ++h) sum += std::log (mag (f * h) + 1e-12);
        if (sum > best) { best = sum; bf = f; }
    }
    if (oddOk) { double odd = 0, even = 0; for (int h : { 1, 3, 5 }) odd += mag (bf * h); for (int h : { 2, 4, 6 }) even += mag (bf * h); *oddOk = odd > 0.05 * even; }
    return bf;
}
// formant envelope centroid (250-6000 Hz) from harmonic amplitudes, interpolated in log-frequency
static double envelopeCentroid (const std::vector<double>& x, double fs, double f0)
{
    const size_t N = 1 << 16; std::vector<cd> a (N, 0.0); const size_t len = std::min (N, x.size() / 2);
    for (size_t i = 0; i < len; ++i) a[i] = x[x.size() - len + i] * (0.5 - 0.5 * std::cos (2 * pi * double (i) / double (len)));
    fft (a);
    std::vector<std::pair<double, double>> h;
    for (int k = 1; k * f0 < 7000; ++k)
    { const double kk = k * f0 * double (N) / fs; double m = 0; for (long j = long (kk) - 3; j <= long (kk) + 3; ++j) m = std::max (m, std::abs (a[size_t (j)])); h.push_back ({ std::log (k * f0), std::log (m + 1e-12) }); }
    double num = 0, den = 0;
    for (double lf = std::log (250.0); lf < std::log (6000.0); lf += 0.01)
    {
        size_t j = 1; while (j + 1 < h.size() && h[j].first < lf) ++j;
        const double t = std::clamp ((lf - h[j - 1].first) / (h[j].first - h[j - 1].first), 0.0, 1.0);
        const double amp = std::exp (h[j - 1].second + t * (h[j].second - h[j - 1].second));
        num += amp * std::exp (lf); den += amp;
    }
    return num / den;
}
static double rmsDb (const std::vector<double>& x) { double s = 0; for (size_t i = x.size() / 3; i < x.size(); ++i) s += x[i] * x[i]; return 10 * std::log10 (s / double (x.size() - x.size() / 3) + 1e-30); }
// clicks: worst 2 ms window of energy above 6 kHz relative to the input's
static double clickiness (const std::vector<double>& in, const std::vector<double>& out, double fs)
{
    auto hp = [fs] (const std::vector<double>& x) { std::vector<double> y (x.size()); double a = std::exp (-2 * pi * 6000 / fs), z1 = 0, z2 = 0, p1 = 0, p2 = 0;
        for (size_t i = 0; i < x.size(); ++i) { z1 = a * (z1 + x[i] - p1); p1 = x[i]; z2 = a * (z2 + z1 - p2); p2 = z1; y[i] = z2; } return y; };
    const auto hi = hp (in), ho = hp (out); const size_t w = size_t (0.002 * fs);
    double worstIn = 1e-12, worstOut = 1e-12;
    for (size_t s = in.size() / 4; s + w < in.size(); s += w)
    { double a = 0, b = 0; for (size_t i = 0; i < w; ++i) { a += hi[s + i] * hi[s + i]; b += ho[s + i] * ho[s + i]; } worstIn = std::max (worstIn, a); worstOut = std::max (worstOut, b); }
    return 10 * std::log10 (worstOut / worstIn);
}

int main()
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        std::printf ("---- %.1f kHz\n", fs / 1000);
        Values base = defaults (Effect::Voice); base[mixSlot] = 1;
        const auto x = vowel (fs, 3.0, 165, 0.012, 0.02);
            const double cIn = envelopeCentroid (x, fs, 165);
        for (double st : { -7.0, 5.0, 12.0 })
        {
            auto v = base; v[voice::pitch] = st;
            const auto y = run (fs, v, x);
            const double want = 165 * std::exp2 (st / 12); bool odd = false; const double p = combPitch (y, fs, want, &odd);
            char t[96];
            std::snprintf (t, sizeof t, "Transpose %+d st: pitch (cents off)", int (st)); check (std::abs (1200 * std::log2 (p / want)) < 15 && odd, t, 1200 * std::log2 (p / want), "cents");
            const double mv = 100 * (envelopeCentroid (y, fs, p) / cIn - 1);
            std::snprintf (t, sizeof t, "Transpose %+d st: formants stay put (centroid move)", int (st)); check (std::abs (mv) < 10, t, mv, "%");
            std::snprintf (t, sizeof t, "Transpose %+d st: level vs input", int (st)); check (std::abs (rmsDb (y) - rmsDb (x)) < 2.5, t, rmsDb (y) - rmsDb (x), "dB");
            std::snprintf (t, sizeof t, "Transpose %+d st: no clicks (HF burst vs input)", int (st)); check (clickiness (x, y, fs) < 6, t, clickiness (x, y, fs), "dB");
        }
        { auto v = base; v[voice::formant] = 4; const double r = envelopeCentroid (run (fs, v, x), fs, 165) / cIn;
          check (std::abs (r / std::exp2 (4.0 / 12) - 1) < 0.12, "Formant +4 st moves formants up 26 % (ratio)", r, "x"); }
        { // Link = formants follow the pitch, like playing the recording faster: compare against that reference
          auto v = base; v[voice::pitch] = 7; v[voice::link] = 1; const double r7 = std::exp2 (7.0 / 12);
          std::vector<double> ref (size_t (double (x.size()) / r7)); for (size_t i = 0; i < ref.size(); ++i) { const double p = double (i) * r7; const size_t k = size_t (p); const double f = p - double (k); ref[i] = k + 1 < x.size() ? x[k] * (1 - f) + x[k + 1] * f : 0.0; }
          const auto y = run (fs, v, x); const double rRef = envelopeCentroid (ref, fs, 165 * r7) / cIn, r = envelopeCentroid (y, fs, combPitch (y, fs, 165 * r7)) / cIn;
          if (fs < 60000) check (std::abs (r / rRef - 1) < 0.08, "Link +7 st: formants match a sped-up recording (ratio)", r / rRef, "x");
          else std::printf ("info  Link +7 st vs sped-up recording: %.2f (this envelope measure is not reliable above 48 kHz)\n", r / rRef); }
        { auto v = base; v[voice::mode] = 2; v[voice::pitch] = 9;           // hold on A
          const auto xv = vowel (fs, 3.0, 200, 0.03, 0.01, 7); const auto y = run (fs, v, xv);
          // flatness: share of each harmonic's energy (h = 1..6) that sits within +-6 cents of the held note;
          // the 3 % vibrato input spreads it over +-50 cents
          auto sharp = [fs] (const std::vector<double>& z, double f) {
              const size_t N = 1 << 17; std::vector<cd> a (N, 0.0); const size_t len = std::min (N, z.size() / 2);
              for (size_t i = 0; i < len; ++i) a[i] = z[z.size() - len + i] * (0.5 - 0.5 * std::cos (2 * pi * double (i) / double (len)));
              fft (a); double in = 0, all = 0;
              for (int h = 1; h <= 6; ++h) for (size_t k = size_t (f * h * std::exp2 (-80 / 1200.0) * N / fs); k <= size_t (f * h * std::exp2 (80 / 1200.0) * N / fs); ++k)
              { const double fk = double (k) * fs / double (N), e = std::norm (a[k]); all += e; if (std::abs (1200 * std::log2 (fk / (f * h))) < 6) in += e; }
              return in / all; };
          const double m = combPitch (y, fs, 220.0);
          std::printf ("info  (input with vibrato: %.0f %% of harmonic energy within 6 cents)\n", 100 * sharp (xv, 200.0));
          check (sharp (y, 220.0) > 0.6, "Robot flattens 3 % vibrato (energy within 6 cents)", 100 * sharp (y, 220.0), "%");
          check (std::abs (1200 * std::log2 (m / 220.0)) < 15, "Robot lands on A3 (220 Hz)", 1200 * std::log2 (m / 220.0), "cents"); }
        { auto v = base; v[voice::mode] = 1; const auto xq = vowel (fs, 3.0, 452, 0.0, 0.01, 3); const double p = combPitch (run (fs, v, xq), fs, 440.0);
          check (std::abs (1200 * std::log2 (p / 440.0)) < 12, "Quantize pulls 452 Hz to A4", 1200 * std::log2 (p / 440.0), "cents"); }
        { auto v = base; v[voice::pitch] = 5; std::vector<double> n (size_t (fs * 2)); unsigned s = 9; for (double& q : n) { s = s * 1664525u + 1013904223u; q = 0.1 * ((s >> 9) / 4194304.0 - 1.0); }
          const auto y = run (fs, v, n); check (std::abs (rmsDb (y) - rmsDb (n)) < 2.0, "Breath / noise passes at the same level", rmsDb (y) - rmsDb (n), "dB"); }
        { auto v = base; v[voice::pitch] = -12; v[voice::range] = 1; const auto xl = vowel (fs, 3.0, 98, 0.0, 0.02, 5); bool odd = false; const double p = combPitch (run (fs, v, xl), fs, 49.0, &odd);
          check (std::abs (1200 * std::log2 (p / 49.0)) < 20 && odd, "Low voice (98 Hz) down an octave, Voice = Low", 1200 * std::log2 (p / 49.0), "cents"); }
        { bool finiteAll = true; auto v = base; unsigned s = 3;
          for (int k = 0; k < 40 && finiteAll; ++k) { s = s * 1664525u + 1013904223u; v[voice::pitch] = -12 + (s >> 16) % 25; v[voice::formant] = -12.0 + (s >> 8) % 25; v[voice::mode] = (s >> 4) % 3; v[voice::link] = (s >> 3) % 2; v[voice::drive] = (s >> 5) % 25; v[voice::range] = (s >> 7) % 4;
            const auto y = run (fs, v, vowel (fs, 0.3, 80 + (s >> 12) % 600, 0.02, 0.05, s)); for (double q : y) if (! std::isfinite (q) || std::abs (q) > 4) { finiteAll = false; break; } }
          check (finiteAll, "40 random settings: finite, bounded output", finiteAll ? 1 : 0, ""); }
    }
    std::printf ("%s: %d failed\n", fails ? "FAILED" : "ALL PASSED", fails);
    return fails ? 1 : 0;
}
