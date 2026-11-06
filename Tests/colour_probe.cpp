#include "DSP/ColourProcessors.h"
#include <cstdio>
#include <complex>
#include <vector>
#include <memory>
using namespace eps;
using cd = std::complex<double>;
const double FS = 48000;
static void fft (std::vector<cd>& a) { size_t n = a.size(); for (size_t i = 1, j = 0; i < n; ++i) { size_t b = n >> 1; for (; j & b; b >>= 1) j ^= b; j ^= b; if (i < j) std::swap (a[i], a[j]); }
  for (size_t len = 2; len <= n; len <<= 1) { double an = -2 * pi / len; cd wl (cos (an), sin (an)); for (size_t i = 0; i < n; i += len) { cd w (1); for (size_t j = 0; j < len / 2; ++j) { cd u = a[i + j], v = a[i + j + len / 2] * w; a[i + j] = u + v; a[i + j + len / 2] = u - v; w *= wl; } } } }
static std::vector<double> spec (const std::vector<double>& x) { size_t N = 65536; std::vector<cd> a (N); size_t off = x.size() - N;
  for (size_t i = 0; i < N; ++i) { double w = .35875 - .48829 * cos (2 * pi * i / N) + .14128 * cos (4 * pi * i / N) - .01168 * cos (6 * pi * i / N); a[i] = x[off + i] * w; } fft (a);
  std::vector<double> p (N / 2); for (size_t i = 0; i < N / 2; ++i) p[i] = std::norm (a[i]); return p; }
static double bp (const std::vector<double>& p, double f) { long k = lround (f * 65536 / FS); double s = 0; for (long i = k - 6; i <= k + 6; ++i) if (i > 0 && i < (long) p.size()) s += p[i]; return s; }
static double dB (double x) { return 10 * log10 (x + 1e-30); }
static std::vector<double> run (Effect e, Values v, const std::vector<double>& x) { auto c = std::make_unique<ColourProcessor>(); c->prepare (e, FS); std::vector<double> o (x.size()); for (size_t i = 0; i < x.size(); ++i) o[i] = c->tick ({ x[i], x[i] }, v).l; return o; }
static std::vector<double> sine (double f, double a, double s) { std::vector<double> x ((size_t) (s * FS)); for (size_t i = 0; i < x.size(); ++i) x[i] = a * sin (2 * pi * f * i / FS); return x; }
static double rms (const std::vector<double>& x, size_t a) { double s = 0; for (size_t i = a; i < x.size(); ++i) s += x[i] * x[i]; return sqrt (s / (x.size() - a)); }
int main()
{
    // latency: impulse peak position
    { Values v = defaults (Effect::Heat); v[heat::drive] = 0; std::vector<double> x (4000, 0.0); x[100] = 0.001; auto o = run (Effect::Heat, v, x);
      size_t b = 0; double m = 0; for (size_t i = 0; i < o.size(); ++i) if (std::abs (o[i]) > m) { m = std::abs (o[i]); b = i; } printf ("HEAT impulse peak at +%zu samples (latency %d)\n", b - 100, ColourProcessor::latency()); }
    for (int e : { 0, 1, 7 })
    { Values v = defaults (Effect (e)); if (e == 0) { v[heat::drive] = 0; v[heat::autogain] = 0; } if (e == 1) { v[valve::input] = 0; v[valve::drive] = 0; } if (e == 7) v[tone::drive] = -15;
      printf ("%s small-signal response (dB):", e == 0 ? "HEAT drive 0" : e == 1 ? "VALVE in 0/drv 0" : "TONE drive -15");
      for (double f : { 30., 100., 1000., 5000., 10000., 16000., 19000. }) { auto x = sine (f, 0.003, 1.0); auto o = run (Effect (e), v, x); printf (" %g:%+.2f", f, 20 * log10 (rms (o, 24000) / rms (x, 24000))); } printf ("\n"); }
    const char* st[] = { "Tape", "Console", "Germanium", "Triode", "Pentode" };
    for (double drv : { 6., 18. }) for (int s = 0; s < 5; ++s)
    { Values v = defaults (Effect::Heat); v[heat::drive] = drv; v[heat::style] = s; auto x = sine (1000, 0.125, 2.0); auto o = run (Effect::Heat, v, x); auto p = spec (o); double f1 = bp (p, 1000);
      double thd = 0; for (int h = 2; h <= 9; ++h) thd += bp (p, 1000 * h);
      printf ("HEAT %-9s drive %2.0f, -18 dBFS: level %+5.1f dB (auto) | THD %5.1f%% | H2 %6.1f H3 %6.1f H4 %6.1f H5 %6.1f dBc\n", st[s], drv, 20 * log10 (rms (o, 48000) / rms (x, 48000)), 100 * sqrt (thd / f1), dB (bp (p, 2000) / f1), dB (bp (p, 3000) / f1), dB (bp (p, 4000) / f1), dB (bp (p, 5000) / f1)); }
    for (int q = 0; q < 3; ++q)
    { Values v = defaults (Effect::Heat); v[heat::drive] = 24; v[heat::style] = 4; v[heat::quality] = q; auto x = sine (7300, 0.5, 2.0); auto o = run (Effect::Heat, v, x); auto p = spec (o);
      double f1 = p[lround (7300.0 * 65536 / FS)], al = 0; for (size_t k = 20; k < p.size(); ++k) { double f = k * FS / 65536, h = f / 7300; if (fabs (h - round (h)) * 7300 < 60) continue; al = std::max (al, p[k]); }
      printf ("HEAT Pentode drive 24, 7.3 kHz at -6 dBFS, quality %d: worst alias %.1f dBc\n", q, dB (al / f1)); }
    { Values v = defaults (Effect::Heat); v[heat::drive] = 0; v[heat::autogain] = 0; v[mixSlot] = 1; std::vector<double> x (48000); for (auto& s : x) s = 0; }
    { Values v = defaults (Effect::Valve); auto x = sine (1000, 0.125, 2.0); auto o = run (Effect::Valve, v, x); auto p = spec (o); double f1 = bp (p, 1000);
      printf ("VALVE default: level %+.1f dB, H2 %.1f H3 %.1f dBc\n", 20 * log10 (rms (o, 48000) / rms (x, 48000)), dB (bp (p, 2000) / f1), dB (bp (p, 3000) / f1)); }
    for (double c : { 0., 4., 10. }) { Values v = defaults (Effect::Crush); v[crush::crush] = c; v[crush::crunch] = 0;
      for (double a : { 0.03, 0.3 }) { auto x = sine (200, a, 3.0); auto o = run (Effect::Crush, v, x); printf ("CRUSH crush %2.0f, input %5.1f dBFS -> output %+6.1f dB (peak %.2f)\n", c, 20 * log10 (a), 20 * log10 (rms (o, 96000) / rms (x, 96000)), *std::max_element (o.begin() + 96000, o.end())); } }
}
