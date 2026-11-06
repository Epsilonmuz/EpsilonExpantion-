// Epsilon Expansion 0.2 verification suite: behaviour of each effect against its design targets.
// Build: c++ -O2 -std=c++17 -ISource -IThirdParty Tests/suite.cpp -o suite && ./suite
#include "DSP/EffectEngine.h"
#include <cstdio>
#include <complex>
#include <vector>
#include <memory>
#include <chrono>
#include <string>
using namespace eps;
using cd = std::complex<double>;
static double FS = 48000;
static int passed = 0, failed = 0;
static void check (bool ok, const std::string& name, double value, const char* unit = "")
{
    printf ("%s %-62s %10.3f %s\n", ok ? "PASS" : "FAIL", name.c_str(), value, unit);
    (ok ? passed : failed)++;
}
// ------------------------------------------------------------------ helpers
static void fft (std::vector<cd>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) { size_t b = n >> 1; for (; j & b; b >>= 1) j ^= b; j ^= b; if (i < j) std::swap (a[i], a[j]); }
    for (size_t len = 2; len <= n; len <<= 1) { const double an = -2 * pi / double (len); const cd wl (std::cos (an), std::sin (an));
        for (size_t i = 0; i < n; i += len) { cd w (1); for (size_t j = 0; j < len / 2; ++j) { const cd u = a[i + j], v = a[i + j + len / 2] * w; a[i + j] = u + v; a[i + j + len / 2] = u - v; w *= wl; } } }
}
struct Out { std::vector<double> l, r; };
static Out run (Effect e, Values v, const std::vector<double>& inL, const std::vector<double>& inR, Transport t = {}, int block = 256)
{
    auto eng = std::make_unique<EffectEngine> (e); eng->prepare (FS, v); eng->setParameters (v, true);
    Out o; o.l.resize (inL.size()); o.r.resize (inL.size());
    std::vector<float> L (static_cast<size_t> (block)), R (static_cast<size_t> (block));
    for (size_t s = 0; s < inL.size(); s += size_t (block))
    {
        const size_t n = std::min<size_t> (size_t (block), inL.size() - s);
        for (size_t i = 0; i < n; ++i) { L[i] = float (inL[s + i]); R[i] = float (inR[s + i]); }
        eng->process (L.data(), R.data(), int (n), t);
        for (size_t i = 0; i < n; ++i) { o.l[s + i] = L[i]; o.r[s + i] = R[i]; }
    }
    return o;
}
static Out runMono (Effect e, Values v, const std::vector<double>& x, Transport t = {}) { return run (e, v, x, x, t); }
static std::vector<double> sine (double f, double amp, double sec) { std::vector<double> x (size_t (sec * FS)); for (size_t i = 0; i < x.size(); ++i) x[i] = amp * std::sin (2 * pi * f * double (i) / FS); return x; }
static std::vector<double> impulse (double sec, size_t at = 100, double amp = 0.9) { std::vector<double> x (size_t (sec * FS), 0.0); x[at] = amp; return x; }
static double rms (const std::vector<double>& x, size_t a, size_t b) { double s = 0; for (size_t i = a; i < b; ++i) s += x[i] * x[i]; return std::sqrt (s / double (std::max<size_t> (1, b - a))); }
static double gainDb (const std::vector<double>& in, const std::vector<double>& out) { return toDb (rms (out, out.size() / 2, out.size()) / rms (in, in.size() / 2, in.size())); }
static std::vector<double> spectrum (const std::vector<double>& x, size_t N = 65536)
{
    std::vector<cd> a (N); const size_t off = x.size() - N;
    for (size_t i = 0; i < N; ++i) { const double w = .35875 - .48829 * std::cos (2 * pi * i / N) + .14128 * std::cos (4 * pi * i / N) - .01168 * std::cos (6 * pi * i / N); a[i] = x[off + i] * w; }
    fft (a); std::vector<double> p (N / 2); for (size_t i = 0; i < N / 2; ++i) p[i] = std::norm (a[i]); return p;
}
static double band (const std::vector<double>& p, double f) { const long k = std::lround (f * 65536 / FS); double s = 0; for (long i = k - 6; i <= k + 6; ++i) if (i > 0 && i < long (p.size())) s += p[size_t (i)]; return s; }
static double harm (Effect e, Values v, double f, double amp, int h)
{ auto x = sine (f, amp, 2.0); auto p = spectrum (runMono (e, v, x).l); return 10 * std::log10 (band (p, f * h) / band (p, f) + 1e-30); }
static double thd (Effect e, Values v, double f, double amp)
{ auto x = sine (f, amp, 2.0); auto p = spectrum (runMono (e, v, x).l); double s = 0; for (int h = 2; h <= 9; ++h) s += band (p, f * h); return 100 * std::sqrt (s / band (p, f)); }
// YIN pitch (median over the second half)
static double yin (const std::vector<double>& x, double fmin = 60, double fmax = 1500)
{
    std::vector<double> est; const int W = 2048, tmax = int (FS / fmin), tmin = int (FS / fmax);
    const size_t first = x.size() > size_t (3 * W + 2 * tmax) ? x.size() / 2 : 0;     // short clips: analyse from the start
    for (size_t s = first; s + size_t (W + tmax + 2) < x.size(); s += 2048)
    {
        std::vector<double> d (size_t (tmax + 2), 0); for (int t = 1; t <= tmax + 1; ++t) { double a = 0; for (int i = 0; i < W; ++i) { const double q = x[s + size_t (i)] - x[s + size_t (i + t)]; a += q * q; } d[size_t (t)] = a; }
        std::vector<double> cm (size_t (tmax + 2), 1); double run = 0; for (int t = 1; t <= tmax + 1; ++t) { run += d[size_t (t)]; cm[size_t (t)] = d[size_t (t)] * t / (run + 1e-30); }
        int best = -1; for (int t = tmin; t <= tmax; ++t) if (cm[size_t (t)] < 0.15) { while (t + 1 <= tmax && cm[size_t (t + 1)] < cm[size_t (t)]) ++t; best = t; break; }
        if (best < 0) best = int (std::min_element (cm.begin() + tmin, cm.begin() + tmax) - cm.begin());
        const double a = cm[size_t (best - 1)], b = cm[size_t (best)], c = cm[size_t (best + 1)];
        est.push_back (FS / (best + 0.5 * (a - c) / (a - 2 * b + c)));
    }
    if (est.empty()) return 0;
    std::nth_element (est.begin(), est.begin() + long (est.size() / 2), est.end()); return est[est.size() / 2];
}
static double cents (double got, double want) { return 1200 * std::log2 (got / want); }
// Schroeder RT60 (T30 extrapolation)
static double rt60 (const std::vector<double>& y, size_t start)
{
    std::vector<double> e (y.size() - start); double acc = 0;
    for (size_t i = y.size(); i-- > start;) { acc += y[i] * y[i]; e[i - start] = acc; }
    size_t t5 = 0, t35 = 0;
    for (size_t i = 0; i < e.size(); ++i) { const double d = 10 * std::log10 (e[i] / e[0] + 1e-30); if (! t5 && d < -5) t5 = i; if (! t35 && d < -35) { t35 = i; break; } }
    return t35 ? 2.0 * double (t35 - t5) / FS : -1;
}
// Metallic ringing: strongest bin relative to the median of its own 1/3-octave neighbourhood (dB), 150 Hz-12 kHz,
// counted only where the tail is within 25 dB of its loudest region (the audible part). Calibration: ideal decaying
// noise scores 11-12 dB; the 0.1.0 plates scored 22-40 dB on the unrestricted version of this metric.
static double ringing (const std::vector<double>& y, size_t a)
{
    std::vector<double> seg (y.begin() + long (a), y.begin() + long (a) + 65536); auto p = spectrum (seg);
    auto localMedian = [&p] (size_t k) { const size_t lo = size_t (double (k) / 1.12), hi = std::min (p.size() - 1, size_t (double (k) * 1.12));
        std::vector<double> nb (p.begin() + long (lo), p.begin() + long (hi)); std::nth_element (nb.begin(), nb.begin() + long (nb.size() / 2), nb.end()); return nb[nb.size() / 2]; };
    const size_t k0 = size_t (150 * 65536 / FS), k1 = size_t (12000 * 65536 / FS);
    double loudest = 0; for (size_t k = k0; k < k1; k += 64) loudest = std::max (loudest, localMedian (k));
    double worst = 0;
    for (size_t k = k0; k < k1; k += 8)
    {
        const double med = localMedian (k);
        if (med < loudest * 0.00316) continue;                      // more than 25 dB below the tail body: inaudible
        double mx = 0; for (size_t j = k; j < std::min (k + 8, p.size()); ++j) mx = std::max (mx, p[j]);
        worst = std::max (worst, mx / med);
    }
    return 10 * std::log10 (worst);
}
static double kurtosis (const std::vector<double>& y, size_t a, size_t b)
{ double m2 = 0, m4 = 0; for (size_t i = a; i < b; ++i) { m2 += y[i] * y[i]; m4 += std::pow (y[i], 4); } m2 /= double (b - a); m4 /= double (b - a); return m4 / (m2 * m2); }
static size_t firstPeak (const std::vector<double>& y, size_t from, double thresh)
{ for (size_t i = from; i < y.size(); ++i) if (std::abs (y[i]) > thresh) { size_t b = i; for (size_t j = i; j < std::min (y.size(), i + 400); ++j) if (std::abs (y[j]) > std::abs (y[b])) b = j; return b; } return 0; }
static double flatness (Effect e, Values v)       // worst deviation (dB) over 40 Hz..16 kHz for a small sine
{ double w = 0; for (double f : { 40., 120., 500., 1000., 3000., 8000., 12000., 16000. }) { auto x = sine (f, 0.003, 0.6); w = std::max (w, std::abs (gainDb (x, runMono (e, v, x).l))); } return w; }

// ------------------------------------------------------------------ tests
static void testHeat()
{
    auto base = defaults (Effect::Heat);
    { auto v = base; v[heat::drive] = 0; v[heat::style] = 3; v[heat::lowcut] = 20; v[heat::autogain] = 0; check (flatness (Effect::Heat, v) < 1.0, "HEAT drive 0 (Triode) flat 40 Hz-16 kHz, worst dB", flatness (Effect::Heat, v), "dB"); }
    { auto v = base; v[heat::drive] = 0; v[heat::style] = 3; v[mixSlot] = 0.5; v[heat::autogain] = 0; const double f = flatness (Effect::Heat, v); check (f < 0.6, "HEAT mix 50 % dry/wet phase-aligned (no comb), worst dB", f, "dB"); }
    { auto v = base; v[heat::drive] = 12; v[heat::style] = 3; const double h2 = harm (Effect::Heat, v, 1000, 0.125, 2), h3 = harm (Effect::Heat, v, 1000, 0.125, 3); check (h2 - h3 > 10, "HEAT Triode is even-order (H2 - H3)", h2 - h3, "dB"); }
    { auto v = base; v[heat::drive] = 18; v[heat::style] = 4; const double h2 = harm (Effect::Heat, v, 1000, 0.125, 2), h3 = harm (Effect::Heat, v, 1000, 0.125, 3); check (h3 - h2 > 20, "HEAT Pentode is odd-order (H3 - H2)", h3 - h2, "dB"); }
    { auto v = base; v[heat::drive] = 12; double t[5]; for (int s = 0; s < 5; ++s) { v[heat::style] = s; t[s] = thd (Effect::Heat, v, 1000, 0.125); }
      check (t[0] < t[1] && t[1] < t[2], "HEAT Tape smoothest < Console < Germanium (THD at drive 12)", t[2] / std::max (1e-3, t[0])); }
    { double worst = 0; auto v = base; for (int s = 0; s < 5; ++s) for (double d : { 0., 12., 24. }) { v[heat::style] = s; v[heat::drive] = d; auto x = sine (1000, 0.125, 1.0); worst = std::max (worst, std::abs (gainDb (x, runMono (Effect::Heat, v, x).l))); }
      check (worst < 2.5, "HEAT Auto holds level across Drive 0-24 dB, all styles", worst, "dB"); }
    { auto v = base; v[heat::drive] = 6; const double a = thd (Effect::Heat, v, 1000, 0.125); v[heat::punish] = 1; const double b = thd (Effect::Heat, v, 1000, 0.125); check (b > 3 * a, "HEAT Punish adds 20 dB of drive (THD ratio)", b / a); }
    { auto v = base; v[heat::drive] = 0; v[heat::highcut] = 2000; auto x = sine (8000, 0.01, 0.6); const double soft = gainDb (x, runMono (Effect::Heat, v, x).l); v[heat::steep] = 1; const double st = gainDb (x, runMono (Effect::Heat, v, x).l);
      check (soft < -9 && soft > -16 && st < -45, "HEAT High Cut 6 dB/oct vs Steep 30 dB/oct (2 oct above)", st, "dB"); }
    { auto v = base; v[heat::drive] = 0; v[heat::lowcut] = 200; auto x = sine (250, 0.01, 0.6); const double a = gainDb (x, runMono (Effect::Heat, v, x).l); v[heat::thump] = 1; const double b = gainDb (x, runMono (Effect::Heat, v, x).l);
      check (b - a > 2.5, "HEAT Thump bumps the Low Cut corner", b - a, "dB"); }
    { auto v = base; v[heat::drive] = 24; v[heat::style] = 4; v[heat::quality] = 2; auto x = sine (7300, 0.5, 2.0); auto p = spectrum (runMono (Effect::Heat, v, x).l);
      double al = 0; for (size_t k = 20; k < size_t (20000.0 * 65536 / FS); ++k) { const double f = double (k) * FS / 65536, h = f / 7300; if (std::abs (h - std::round (h)) * 7300 < 60) continue; al = std::max (al, p[k]); }
      const double r = 10 * std::log10 (al / p[size_t (std::lround (7300.0 * 65536 / FS))]); check (r < -70, "HEAT Ultra 8x audible aliasing (< 20 kHz), Pentode drive 24, 7.3 kHz", r, "dBc"); }
}
static void testValve()
{
    auto v = defaults (Effect::Valve);
    { auto x = sine (1000, 0.125, 1.0); const double g = gainDb (x, runMono (Effect::Valve, v, x).l); check (std::abs (g) < 1.5, "VALVE default unity gain (Auto)", g, "dB"); }
    { auto w = v; w[valve::input] = 0; w[valve::drive] = 0; w[valve::bass] = 10; auto x = sine (50, 0.003, 1.0); const double g = gainDb (x, runMono (Effect::Valve, w, x).l); check (g > 6, "VALVE Bass +10 lifts 50 Hz", g, "dB"); }
    { auto w = v; w[valve::input] = 0; w[valve::drive] = 0; w[valve::treble] = -10; auto x = sine (10000, 0.003, 1.0); const double g = gainDb (x, runMono (Effect::Valve, w, x).l); check (g < -6, "VALVE Treble -10 cuts 10 kHz", g, "dB"); }
    { auto w = v; w[valve::input] = 20; const double a = harm (Effect::Valve, w, 1000, 0.125, 2); check (a > -30, "VALVE Input drives a single-ended (H2) stage", a, "dBc"); }
    { auto w = v; w[valve::noise] = 1; w[valve::mic] = 1; std::vector<double> z (size_t (FS), 0.0); auto o = runMono (Effect::Valve, w, z); const double n = toDb (rms (o.l, size_t (FS / 2), size_t (FS)));
      w[valve::noise] = 0; auto o2 = runMono (Effect::Valve, w, z); check (n > -95 && n < -65 && rms (o2.l, size_t (FS / 2), size_t (FS)) < 1e-7, "VALVE Noisy adds a low hiss/hum; Clean is silent", n, "dBFS"); }
}
static void testCrush()
{
    auto v = defaults (Effect::Crush); v[crush::crush] = 10; v[crush::crunch] = 0;
    { auto q = sine (200, 0.01, 4.0), l = sine (200, 0.3, 4.0); const double a = toDb (rms (runMono (Effect::Crush, v, q).l, size_t (3 * FS), size_t (4 * FS))), b = toDb (rms (runMono (Effect::Crush, v, l).l, size_t (3 * FS), size_t (4 * FS)));
      check (std::abs (a - b) < 4, "CRUSH locks the level (-40 vs -10 dBFS in, output difference)", std::abs (a - b), "dB"); }
    {   // release: after a loud burst the gain recovers; Fast recovers more than Slow in the same time
        auto rec = [&] (int rel) { auto w = v; w[crush::release] = rel; std::vector<double> x (size_t (FS * 6)); for (size_t i = 0; i < x.size(); ++i) x[i] = (i < size_t (FS * 2) ? 0.5 : 0.01) * std::sin (2 * pi * 200 * double (i) / FS);
            auto o = runMono (Effect::Crush, w, x).l; return toDb (rms (o, size_t (FS * 5.5), size_t (FS * 6)) / rms (o, size_t (FS * 2.05), size_t (FS * 2.55))); };
        const double slow = rec (0), fast = rec (1); check (fast > slow + 3 && slow > 0.5, "CRUSH Fast release recovers more than Slow in 3.5 s", fast - slow, "dB"); }
    { auto w = v; w[crush::darkness] = 10; auto x = sine (6000, 0.05, 2.0); const double dk = gainDb (x, runMono (Effect::Crush, w, x).l); w[crush::darkness] = 0; const double br = gainDb (x, runMono (Effect::Crush, w, x).l);
      check (br - dk > 12, "CRUSH Darkness 10 darkens 6 kHz", br - dk, "dB"); }
    { auto w = v; w[crush::crunch] = 0; const double a = thd (Effect::Crush, w, 200, 0.3); w[crush::crunch] = 8; const double b = thd (Effect::Crush, w, 200, 0.3); check (b > a + 5, "CRUSH Crunch distorts (THD %)", b, "%"); }
}
static void testPrism()
{
    auto base = defaults (Effect::Prism); base[mixSlot] = 1; base[prism::recycle] = 0; base[prism::reverse] = 0; base[prism::delay] = 0;
    for (double st : { 12., 7., -12., -5. })
    { auto v = base; v[prism::pitch] = st; v[prism::splice] = 300; auto x = sine (220, 0.3, 4.0); const double c = cents (yin (runMono (Effect::Prism, v, x).l), 220 * std::exp2 (st / 12));
      check (std::abs (c) < 6, "PRISM pitch " + std::to_string (int (st)) + " st in tune (forward splices)", c, "cents"); }
    { auto v = base; v[prism::pitch] = 0; v[prism::reverse] = 1; v[prism::splice] = 500; v[prism::smoothing] = 0.1;
      // a rising chirp comes back falling within each splice when reversed
      std::vector<double> x (size_t (FS * 3)); double ph = 0; for (size_t i = 0; i < x.size(); ++i) { const double f = 300 + 600 * std::fmod (double (i) / FS, 0.5) / 0.5; ph += 2 * pi * f / FS; x[i] = 0.3 * std::sin (ph); }
      auto o = runMono (Effect::Prism, v, x).l; int falling = 0, rising = 0;
      for (size_t s = size_t (FS * 1.5); s + 4096 < size_t (FS * 2.9); s += 2400) { const double a = yin (std::vector<double> (o.begin() + long (s), o.begin() + long (s + 4096)), 150, 1500); const double b = yin (std::vector<double> (o.begin() + long (s + 1200), o.begin() + long (s + 1200 + 4096)), 150, 1500); (b < a ? falling : rising)++; }
      check (falling > rising, "PRISM Reverse plays splices backwards (falling vs rising)", double (falling - rising)); }
    { auto v = base; v[prism::recycle] = 0.95; v[prism::pitch] = 12; auto x = sine (220, 0.9, 6.0); auto o = runMono (Effect::Prism, v, x); double pk = 0; for (double s : o.l) pk = std::max (pk, std::abs (s));
      check (pk < 4.0 && std::isfinite (pk), "PRISM Recycle 95 % stays bounded (peak)", pk); }
    { auto v = base; v[prism::gateduck] = 1; v[prism::threshold] = -30; auto x = sine (220, 0.3, 3.0); const double duck = gainDb (x, runMono (Effect::Prism, v, x).l); v[prism::gateduck] = 0; const double open = gainDb (x, runMono (Effect::Prism, v, x).l);
      check (open - duck > 20, "PRISM Duck removes the effect while input is above Threshold", open - duck, "dB"); }
}
static void testTime()
{
    auto base = defaults (Effect::Time); base[mixSlot] = 1; base[time::feedbackA] = 0; base[time::feedbackB] = 0; base[time::depth] = 0; base[time::timeA] = 250; base[time::timeB] = 375;
    auto x = impulse (3.0);
    { auto o = runMono (Effect::Time, base, x); const double a = double (firstPeak (o.l, 200, 0.05) - 100) / FS * 1000, b = double (firstPeak (o.r, 200, 0.05) - 100) / FS * 1000;
      check (std::abs (a - 250) < 2 && std::abs (b - 375) < 2, "TIME line A 250 ms (left), B 375 ms (right): A error", a - 250, "ms"); }
    for (int m = 1; m <= 3; ++m)
    { auto v = base; v[time::multiply] = m; auto xx = impulse (2.5 * (1 << m) + 0.5); auto o = runMono (Effect::Time, v, xx); const double a = double (firstPeak (o.l, 200, 0.02) - 100) / FS * 1000;
      check (std::abs (a - 250.0 * (1 << m)) < 4 * (1 << m), "TIME Multiply " + std::to_string (1 << m) + "x multiplies the delay", a, "ms"); }
    { auto v = base; v[time::multiply] = 3; auto s = sine (3000, 0.3, 5.0); const double g = gainDb (s, runMono (Effect::Time, v, s).l); check (g < -20, "TIME 8x bandwidth (1.5 kHz) removes 3 kHz", g, "dB"); }
    { auto v = base; v[time::sync] = 1; v[time::noteA] = 3; Transport t; t.hostTempo = true; t.bpm = 100; auto o = runMono (Effect::Time, v, x, t); const double a = double (firstPeak (o.l, 200, 0.05) - 100) / FS * 1000;
      check (std::abs (a - 600) < 2, "TIME Sync 1/4 at 100 BPM = 600 ms", a, "ms"); }
    {   // freeze: capture a loop and hold it for 30 s
        auto v = base; v[time::feedbackA] = 0.5; v[time::timeA] = 400; std::vector<double> in (size_t (FS * 34) / 256 * 256 + 256, 0.0);
        for (size_t i = 0; i < size_t (FS * 2); ++i) in[i] = 0.3 * std::sin (2 * pi * 330 * double (i) / FS);
        auto eng = std::make_unique<EffectEngine> (Effect::Time); eng->prepare (FS, v); std::vector<float> L (256), R (256); std::vector<double> out (in.size());
        for (size_t s = 0; s < in.size(); s += 256) { if (s == size_t (FS * 2) / 256 * 256) { v[time::freeze] = 1; eng->setParameters (v); } for (size_t i = 0; i < 256; ++i) L[i] = R[i] = float (in[s + i]); eng->process (L.data(), R.data(), 256); for (size_t i = 0; i < 256; ++i) out[s + i] = L[i]; }
        const double a = rms (out, size_t (FS * 3), size_t (FS * 4)), b = rms (out, size_t (FS * 33), size_t (FS * 34));
        check (a > 0.01 && b / a > 0.8 && b / a < 1.25, "TIME Freeze holds the loop for 30 s (power ratio)", b / a); }
    { auto v = base; v[time::mode] = 1; v[time::feedbackA] = 0.6; v[time::timeA] = 200; v[time::timeB] = 200; v[time::levelB] = 1; auto o = runMono (Effect::Time, v, x);
      const size_t l1 = firstPeak (o.l, 200, 0.02), r1 = firstPeak (o.r, 200, 0.02); check (r1 > l1 + size_t (0.15 * FS), "TIME Ping-Pong alternates left then right", double (r1 - l1) / FS * 1000, "ms"); }
}
static void testPlate()
{
    const char* names[] = { "Steel", "Gold Foil", "Bright", "Warm", "Splash" };
    for (int m = 0; m < 5; ++m)
    {
        double worst = 0, ring = 0, kurt = 0;
        for (double d : { 1.0, 2.5, 6.0 })
        {
            auto v = defaults (Effect::Plate); v[mixSlot] = 1; v[plate::decay] = d; v[plate::model] = m; v[plate::predelay] = 0; v[plate::lowcut] = 20; v[plate::highcut] = 20000; v[plate::damping] = 20000;
            auto x = impulse (d * 2.2 + 1.5); auto o = runMono (Effect::Plate, v, x);
            // RT60 measured in the 500 Hz region (band-pass the response first)
            Svf bp; bp.set (Svf::BandPass, FS, 500, 1.0); std::vector<double> y (o.l.size()); for (size_t i = 0; i < y.size(); ++i) y[i] = bp.tick (o.l[i]);
            const double r = rt60 (y, 100); worst = std::max (worst, std::abs (r / d - 1));
            if (d == 2.5)
            {
                kurt = kurtosis (o.l, size_t (0.05 * FS), size_t (0.15 * FS));
                auto w = defaults (Effect::Plate); w[mixSlot] = 1; w[plate::decay] = d; w[plate::model] = m; w[plate::predelay] = 0;
                ring = ringing (runMono (Effect::Plate, w, x).l, size_t (0.1 * FS));
            }
        }
        check (worst < 0.2, std::string ("PLATE ") + names[m] + " RT60 at 500 Hz tracks Decay (1, 2.5, 6 s), worst error", worst * 100, "%");
        check (ring < 20, std::string ("PLATE ") + names[m] + " tail free of metallic ringing (peak over local median)", ring, "dB");
        check (kurt < 5, std::string ("PLATE ") + names[m] + " dense early tail (kurtosis 50-150 ms)", kurt);
    }
    {   auto v = defaults (Effect::Plate); v[mixSlot] = 1; std::vector<double> in (size_t (FS * 34) / 256 * 256 + 256, 0.0); for (size_t i = 0; i < size_t (FS); ++i) in[i] = 0.3 * std::sin (2 * pi * 440 * double (i) / FS);
        auto eng = std::make_unique<EffectEngine> (Effect::Plate); eng->prepare (FS, v); std::vector<float> L (256), R (256); std::vector<double> out (in.size());
        for (size_t s = 0; s < in.size(); s += 256) { if (s == size_t (FS * 1.2) / 256 * 256) { v[plate::freeze] = 1; eng->setParameters (v); } for (size_t i = 0; i < 256; ++i) L[i] = R[i] = float (in[s + i]); eng->process (L.data(), R.data(), 256); for (size_t i = 0; i < 256; ++i) out[s + i] = L[i]; }
        const double a = rms (out, size_t (FS * 3), size_t (FS * 4)), b = rms (out, size_t (FS * 33), size_t (FS * 34));
        check (a > 1e-3 && b / a > 0.7 && b / a < 1.3, "PLATE Freeze sustains for 30 s (power ratio)", b / a); }
    {   auto v = defaults (Effect::Plate); v[mixSlot] = 1; auto x = impulse (3.0); auto o = runMono (Effect::Plate, v, x); double a = 0, b = 0, c = 0;
        for (size_t i = 2000; i < size_t (FS * 2); ++i) { a += o.l[i] * o.r[i]; b += o.l[i] * o.l[i]; c += o.r[i] * o.r[i]; }
        check (std::abs (a / std::sqrt (b * c)) < 0.3, "PLATE wide stereo tail (L/R correlation)", a / std::sqrt (b * c)); }
    {   auto v = defaults (Effect::Plate); v[mixSlot] = 1; v[plate::decay] = 4; v[plate::autodecay] = 1; std::vector<double> x (size_t (FS * 3)); for (size_t i = 0; i < x.size(); ++i) x[i] = 0.3 * std::sin (2 * pi * 300 * double (i) / FS) * (i < size_t (FS * 2) ? 1.0 : 0.0);
        auto o1 = runMono (Effect::Plate, v, x); v[plate::autodecay] = 0; auto o2 = runMono (Effect::Plate, v, x);
        const double a = rms (o1.l, size_t (FS * 2.05), size_t (FS * 2.4)), b = rms (o2.l, size_t (FS * 2.05), size_t (FS * 2.4));
        check (a < b * 0.7, "PLATE Auto-Decay: shorter tail after a loud phrase", toDb (a / b), "dB"); }
}
static void testSpace()
{
    auto energyCentroid = [] (const std::vector<double>& y, size_t a, size_t b) { double s = 0, w = 0; for (size_t i = a; i < b; ++i) { s += y[i] * y[i] * double (i - a); w += y[i] * y[i]; } return s / w / double (b - a); };
    const double T = 3.0;
    auto base = defaults (Effect::Space); base[mixSlot] = 1; base[space::time] = T; base[space::mod] = 0; base[space::texture] = 0.6;
    { auto v = base; v[space::shape] = 0; v[space::contour] = 1; auto o = runMono (Effect::Space, v, impulse (T + 2)); const double c = energyCentroid (o.l, 100, size_t (T * FS));
      check (c < 0.3, "SPACE Shape decay: energy front-loaded (centroid / T)", c); }
    { auto v = base; v[space::shape] = 1; v[space::contour] = 1; auto o = runMono (Effect::Space, v, impulse (T + 2)); const double c = energyCentroid (o.l, 100, size_t (T * FS));
      check (c > 0.65, "SPACE Shape reverse: energy builds to the end (centroid / T)", c); }
    { auto v = base; v[space::shape] = 0.5; v[space::contour] = 1; auto o = runMono (Effect::Space, v, impulse (T + 2)); const double c = energyCentroid (o.l, 100, size_t (T * FS));
      check (c > 0.4 && c < 0.66, "SPACE Shape bloom: swell in the middle (centroid / T)", c); }
    { auto v = base; v[space::contour] = 0; v[space::texture] = 0.3; auto o = runMono (Effect::Space, v, impulse (T + 2)); const double inside = rms (o.l, size_t (0.5 * FS), size_t (T * FS)), after = rms (o.l, size_t ((T + 0.6) * FS), size_t ((T + 1.6) * FS));
      check (toDb (after / inside) < -30, "SPACE Contour 0 gates: level after the window", toDb (after / inside), "dB"); }
    { auto v = base; v[space::time] = 20; v[space::shape] = 0.5; v[space::contour] = 1; auto o = runMono (Effect::Space, v, impulse (24)); const double c = energyCentroid (o.l, 100, size_t (20 * FS));
      check (c > 0.4 && c < 0.6, "SPACE 20 s window keeps its shape (centroid / T)", c); }
    {   auto v = base; v[space::time] = 2; std::vector<double> in (size_t (FS * 34) / 256 * 256 + 256, 0.0); for (size_t i = 0; i < size_t (FS * 2); ++i) in[i] = 0.3 * std::sin (2 * pi * 440 * double (i) / FS);
        auto eng = std::make_unique<EffectEngine> (Effect::Space); eng->prepare (FS, v); std::vector<float> L (256), R (256); std::vector<double> out (in.size());
        for (size_t s = 0; s < in.size(); s += 256) { if (s == size_t (FS * 2.5) / 256 * 256) { v[space::freeze] = 1; eng->setParameters (v); } for (size_t i = 0; i < 256; ++i) L[i] = R[i] = float (in[s + i]); eng->process (L.data(), R.data(), 256); for (size_t i = 0; i < 256; ++i) out[s + i] = L[i]; }
        const double a = rms (out, size_t (FS * 4), size_t (FS * 6)), b = rms (out, size_t (FS * 32), size_t (FS * 34));
        check (a > 1e-3 && b / a > 0.6 && b / a < 1.5, "SPACE Freeze keeps the space going for 30 s (power ratio)", b / a); }
    { auto v = base; v[space::colour] = 1; auto o = runMono (Effect::Space, v, impulse (T + 2)); auto ob = runMono (Effect::Space, base, impulse (T + 2));
      auto hf = [] (const std::vector<double>& y, size_t a, size_t b) { double s = 0; for (size_t i = a + 1; i < b; ++i) s += (y[i] - y[i - 1]) * (y[i] - y[i - 1]); return s; };
      const double late = hf (o.l, size_t (2 * FS), size_t (T * FS)) / hf (ob.l, size_t (2 * FS), size_t (T * FS)), early = hf (o.l, size_t (0.05 * FS), size_t (0.8 * FS)) / hf (ob.l, size_t (0.05 * FS), size_t (0.8 * FS));
      check (late > early * 1.5, "SPACE Color brightens progressively over time (late/early HF ratio)", late / early); }
}
static void testTone()
{
    auto v = defaults (Effect::Tone);
    { auto w = v; w[tone::colour] = 0; check (flatness (Effect::Tone, w) < 0.3, "TONE all controls at 0 (amp off) is flat", flatness (Effect::Tone, w), "dB"); }
    { check (flatness (Effect::Tone, v) < 0.6, "TONE amp on, Drive 0: flat 40 Hz-16 kHz", flatness (Effect::Tone, v), "dB"); }
    { auto w = v; w[tone::low] = 15; auto x = sine (40, 0.003, 1.0); const double g = gainDb (x, runMono (Effect::Tone, w, x).l); check (g > 10, "TONE Low +15 lifts 40 Hz (low bell)", g, "dB"); }
    { auto w = v; w[tone::high] = -15; auto x = sine (12000, 0.003, 1.0); const double g = gainDb (x, runMono (Effect::Tone, w, x).l); check (g < -8, "TONE High -15 rolls off 12 kHz", g, "dB"); }
    static constexpr double mids[6] = { 700, 1000, 1500, 2300, 3500, 5600 };
    for (int i = 0; i < 6; ++i)
    { auto w = v; w[tone::colour] = 0; w[tone::mid] = 8; w[tone::midfreq] = i; double best = 0, bf = 0; for (double f = mids[i] * 0.7; f < mids[i] * 1.4; f *= 1.02) { auto x = sine (f, 0.003, 0.3); const double g = gainDb (x, runMono (Effect::Tone, w, x).l); if (g > best) { best = g; bf = f; } }
      check (std::abs (bf / mids[i] - 1) < 0.06 && best > 7, "TONE Mid +8 peaks at " + std::to_string (int (mids[i])) + " Hz", bf, "Hz"); }
    { double worst = 0; for (double d : { -15., 0., 15. }) { auto w = v; w[tone::drive] = d; auto x = sine (1000, 0.125, 1.0); worst = std::max (worst, std::abs (gainDb (x, runMono (Effect::Tone, w, x).l))); }
      check (worst < 1.5, "TONE Drive -15..+15 with automatic level compensation", worst, "dB"); }
}
static void testVoice()
{
    auto base = defaults (Effect::Voice); base[mixSlot] = 1;
    auto saw = [] (double f, double sec) { std::vector<double> x (size_t (sec * FS)); for (size_t i = 0; i < x.size(); ++i) x[i] = 0.3 * (2 * wrap (f * double (i) / FS) - 1); return x; };
    for (double st : { 7., -12., 12. })
    { auto v = base; v[voice::pitch] = st; const double c = cents (yin (runMono (Effect::Voice, v, saw (220, 3)).l), 220 * std::exp2 (st / 12)); check (std::abs (c) < 10, "VOICE Transpose " + std::to_string (int (st)) + " st in tune", c, "cents"); }
    { auto v = base; v[voice::mode] = 1; const double c = cents (yin (runMono (Effect::Voice, v, saw (452, 3)).l), 440); check (std::abs (c) < 12, "VOICE Quantize snaps 452 Hz to A4 (440 Hz)", c, "cents"); }
    { auto v = base; v[voice::mode] = 2; v[voice::pitch] = 0; const double a = yin (runMono (Effect::Voice, v, saw (220, 3)).l), b = yin (runMono (Effect::Voice, v, saw (247, 3)).l);
      const double ca = std::remainder (cents (a, 261.63), 1200.0), cb = std::remainder (cents (b, 261.63), 1200.0); check (std::abs (ca) < 15 && std::abs (cb) < 15, "VOICE Robot holds A3 and B3 on the note C", std::max (std::abs (ca), std::abs (cb)), "cents"); }
    { auto v = base; v[mixSlot] = 0.5; double w = 0; for (double f : { 300., 1000., 3000. }) { auto x = sine (f, 0.1, 2.0); w = std::max (w, std::abs (gainDb (x, runMono (Effect::Voice, v, x).l))); }
      check (w < 1.5, "VOICE Mix 50 % at Pitch 0: dry and wet aligned", w, "dB"); }
}
static void testCommon()
{
    const char* names[] = { "HEAT", "VALVE", "CRUSH", "PRISM", "TIME", "PLATE", "SPACE", "TONE", "VOICE" };
    for (int e = 0; e < 9; ++e)
    {
        auto v = defaults (Effect (e)); if (e == 0) v[heat::quality] = 1;
        std::vector<double> x (size_t (FS * 4)); Noise n; for (auto& s : x) s = 0.1 * n.next();
        const auto t0 = std::chrono::steady_clock::now(); runMono (Effect (e), v, x); const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        check (sec / 4 < 0.25, std::string (names[e]) + " CPU per stereo instance (share of one core)", 100 * sec / 4, "%");
        // bypass is the latency-aligned dry signal
        auto w = v; w[bypassSlot] = 1; auto s = sine (1000, 0.2, 1.0); const double g = gainDb (s, runMono (Effect (e), w, s).l);
        check (std::abs (g) < 0.05, std::string (names[e]) + " Bypass passes the dry signal", g, "dB");
    }
}
int main (int argc, char** argv)
{
    if (argc > 1) FS = std::atof (argv[1]);
    printf ("Epsilon Expansion 0.2 verification at %.0f Hz\n", FS);
    testHeat(); testValve(); testCrush(); testPrism(); testTime(); testPlate(); testSpace(); testTone(); testVoice(); testCommon();
    printf ("TOTAL %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
