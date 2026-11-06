#pragma once
// Shared DSP building blocks. Header-only, no JUCE, no allocation after prepare().
#include "Definitions.h"
#include <vector>
#include <cstdint>

namespace eps {
inline double finite (double x) { return std::isfinite (x) ? x : 0.0; }

// ------------------------------------------------------------------ circular buffer with fractional reads
class Ring
{
public:
    void prepare (size_t n) { v.assign (std::max<size_t> (16, n), 0.0f); pos = 0; }
    void reset() { std::fill (v.begin(), v.end(), 0.0f); pos = 0; }
    size_t size() const noexcept { return v.size(); }
    void push (double x) noexcept { v[pos] = float (x); pos = pos + 1 == v.size() ? 0 : pos + 1; }
    // sample written 'delay' samples ago (delay >= 0, fractional, linear interpolation)
    double age (double delay) const noexcept
    {
        delay = std::clamp (delay, 0.0, double (v.size() - 3));
        const auto d = size_t (delay);
        const double f = delay - double (d);
        const size_t n = v.size();
        const size_t i0 = (pos + n - 1 - d) % n, i1 = (i0 + n - 1) % n;
        return v[i0] + f * (v[i1] - v[i0]);
    }
    // cubic (Hermite) read, for pitch shifting and modulated delays
    double ageCubic (double delay) const noexcept
    {
        delay = std::clamp (delay, 1.0, double (v.size() - 4));
        const auto d = size_t (delay);
        const double f = delay - double (d);
        const size_t n = v.size(), i0 = (pos + n - 1 - d) % n;
        const double ym1 = v[(i0 + 1) % n], y0 = v[i0], y1 = v[(i0 + n - 1) % n], y2 = v[(i0 + n - 2) % n];
        const double c1 = .5 * (y1 - ym1), c2 = ym1 - 2.5 * y0 + 2 * y1 - .5 * y2, c3 = .5 * (y2 - ym1) + 1.5 * (y0 - y1);
        return ((c3 * f + c2) * f + c1) * f + y0;
    }
    float& raw (size_t i) noexcept { return v[i % v.size()]; }
    size_t writeIndex() const noexcept { return pos; }
private:
    std::vector<float> v;
    size_t pos = 0;
};

struct OnePole
{
    double s = 0;
    double lp (double x, double a) noexcept { s += a * (x - s); return s; }      // a = 1 - exp(-2 pi fc / fs)
    void reset() { s = 0; }
    static double coeff (double fc, double fs) { return 1 - std::exp (-2 * pi * std::clamp (fc, 1.0, 0.49 * fs) / fs); }
};

struct DcBlock
{
    double x1 = 0, y1 = 0;
    double tick (double x, double r) noexcept { const double y = x - x1 + r * y1; x1 = x; y1 = y; return y; }
    void reset() { x1 = y1 = 0; }
};

// ------------------------------------------------------------------ TPT state-variable filter (Zavalishin / Simper):
// stable and click-free under fast modulation.
class Svf
{
public:
    enum Mode { LowPass, HighPass, BandPass, Bell, LowShelf, HighShelf, Notch };
    void set (Mode m, double fs, double fc, double q = 0.7071, double db = 0) noexcept
    {
        mode = m;
        fc = std::clamp (fc, 5.0, 0.49 * fs);
        const double A = std::pow (10.0, db / 40.0);
        double g = std::tan (pi * fc / fs);
        if (m == LowShelf) g /= std::sqrt (A);
        if (m == HighShelf) g *= std::sqrt (A);
        k = m == Bell ? 1.0 / (q * A) : 1.0 / q;
        a1 = 1 / (1 + g * (g + k)); a2 = g * a1; a3 = g * a2;
        switch (m)
        {
            case LowPass:   m0 = 0; m1 = 0; m2 = 1; break;
            case HighPass:  m0 = 1; m1 = -k; m2 = -1; break;
            case BandPass:  m0 = 0; m1 = 1; m2 = 0; break;            // peak gain 1/k... normalised below
            case Notch:     m0 = 1; m1 = -k; m2 = 0; break;
            case Bell:      m0 = 1; m1 = k * (A * A - 1); m2 = 0; break;
            case LowShelf:  m0 = 1; m1 = k * (A - 1); m2 = A * A - 1; break;
            case HighShelf: m0 = A * A; m1 = k * (1 - A) * A; m2 = 1 - A * A; break;
        }
        if (m == BandPass) m1 = k;                                     // unity gain at fc
    }
    double tick (double x) noexcept
    {
        const double v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
        return m0 * x + m1 * v1 + m2 * v2;
    }
    void reset() { ic1 = ic2 = 0; }
private:
    Mode mode = LowPass;
    double k = 1.4142, a1 = 0, a2 = 0, a3 = 0, m0 = 0, m1 = 0, m2 = 1, ic1 = 0, ic2 = 0;
};

// Butterworth cascade (orders 1..6) built from SVF sections, for low/high cut with selectable slope.
class CutFilter
{
public:
    void set (bool high, int order, double fs, double fc) noexcept
    {
        order = std::clamp (order, 1, 6);
        n = 0;
        firstOrder = order & 1;
        if (firstOrder) { a1 = OnePole::coeff (fc, fs); }
        isHigh = high;
        for (int i = 1; i <= order / 2; ++i)
        {
            const double theta = (order & 1) ? i * pi / order : (2.0 * i - 1.0) * pi / (2.0 * order);
            sec[size_t (n++)].set (high ? Svf::HighPass : Svf::LowPass, fs, fc, 1.0 / (2.0 * std::cos (theta)));
        }
    }
    double tick (double x) noexcept
    {
        if (firstOrder) { const double l = one.lp (x, a1); x = isHigh ? x - l : l; }
        for (int i = 0; i < n; ++i) x = sec[size_t (i)].tick (x);
        return x;
    }
    void reset() { one.reset(); for (auto& s : sec) s.reset(); }
private:
    std::array<Svf, 3> sec;
    OnePole one;
    double a1 = 1;
    int n = 0;
    bool firstOrder = false, isHigh = false;
};

// ------------------------------------------------------------------ oversampling
// Cascade of linear-phase half-band FIR stages (2x, 4x, 8x). Group delays are chosen so the
// total is a whole number of base-rate samples, so the dry path can be aligned exactly.
// Passband is flat to 20 kHz at 44.1/48 kHz; images and aliases are suppressed by > 90 dB.
class HalfBand
{
public:
    explicit HalfBand (int taps = 0) { if (taps) design (taps); }
    void design (int taps)
    {
        n = taps;                                      // odd
        h.assign (size_t (n), 0.0);
        const int c = (n - 1) / 2;
        const double beta = 9.0;                      // Kaiser window
        auto bessel = [] (double x) { double s = 1, t = 1; for (int k = 1; k < 30; ++k) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; } return s; };
        double sum = 0;
        for (int i = 0; i < n; ++i)
        {
            const int m = i - c;
            const double sinc = m == 0 ? 0.5 : std::sin (pi * m / 2.0) / (pi * m);
            const double r = double (m) / c;
            const double w = bessel (beta * std::sqrt (std::max (0.0, 1 - r * r))) / bessel (beta);
            h[size_t (i)] = sinc * w;
            sum += h[size_t (i)];
        }
        for (auto& x : h) x /= sum;
        // odd-offset taps only (plus the centre): store them for the polyphase form
        odd.clear();
        for (int i = 0; i < n; ++i) if (((i - c) & 1) != 0) odd.push_back (h[size_t (i)]);
        centre = h[size_t (c)];
        buf.assign (size_t (n) * 2, 0.0);
        delay = c;
    }
    int groupDelay() const { return delay; }      // in high-rate samples
    void reset() { std::fill (buf.begin(), buf.end(), 0.0); p = 0; }
    // push one high-rate sample, get one filtered high-rate sample
    double tick (double x) noexcept
    {
        buf[size_t (p)] = buf[size_t (p + n)] = x;
        const double* s = &buf[size_t (p + 1)];       // s[0] = oldest ... s[n-1] = newest
        double y = centre * s[(n - 1) / 2];
        for (size_t j = 0, i = 0; i < size_t (n); ++i)
            if (((int (i) - (n - 1) / 2) & 1) != 0) y += odd[j++] * s[n - 1 - int (i)];
        p = p + 1 == n ? 0 : p + 1;
        return y;
    }
private:
    std::vector<double> h, odd, buf;
    double centre = 0.5;
    int n = 0, p = 0, delay = 0;
};

template <typename Fn>
class Oversampler
{
public:
    // stages: 1 (2x), 2 (4x), 3 (8x)
    void prepare()
    {
        constexpr int taps[3] = { 91, 33, 33 };       // group delays 45, 16, 16 -> 45 + 16/2 + 16/4 = 57 base samples
        for (int s = 0; s < 3; ++s) { up[size_t (s)].design (taps[s]); down[size_t (s)].design (taps[s]); }
        reset();
    }
    static int latency (int stages)
    {
        constexpr int gd[3] = { 45, 16, 16 };
        int l = 0; for (int s = 0; s < stages; ++s) l += gd[s] >> s;
        return l;
    }
    static constexpr int maxLatency = 57;
    void reset() { for (auto& f : up) f.reset(); for (auto& f : down) f.reset(); }

    // process one base-rate sample through 'stages' of oversampling around fn(x)
    double tick (double x, int stages, Fn& fn) noexcept { return run (x, 0, stages, fn); }

private:
    double run (double x, int s, int stages, Fn& fn) noexcept
    {
        if (s >= stages) return fn (x);
        // upsample by 2: zero-stuff, filter (gain 2)
        const double a = up[size_t (s)].tick (2 * x), b = up[size_t (s)].tick (0.0);
        const double ya = run (a, s + 1, stages, fn), yb = run (b, s + 1, stages, fn);
        // keep the output aligned with the even sample: total delay is then a whole number of base samples
        const double keep = down[size_t (s)].tick (ya);
        down[size_t (s)].tick (yb);
        return keep;
    }
    std::array<HalfBand, 3> up, down;
};

// fixed integer delay for latency alignment
class IntDelay
{
public:
    void prepare (int maxDelay) { buf.assign (size_t (maxDelay + 1), 0.0); p = 0; }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0); p = 0; }
    double tick (double x, int d) noexcept
    {
        const int n = int (buf.size());
        buf[size_t (p)] = x;
        const int r = (p - std::clamp (d, 0, n - 1) + n) % n;
        p = p + 1 == n ? 0 : p + 1;
        return buf[size_t (r)];
    }
private:
    std::vector<double> buf;
    int p = 0;
};

// smooth parameter (one-pole, per sample)
struct Smooth
{
    double v = 0, a = 0.001;
    void prepare (double fs, double ms, double initial) { a = 1 - std::exp (-1.0 / (0.001 * ms * fs)); v = initial; }
    double next (double target) noexcept { v += a * (target - v); return v; }
};

// envelope follower with separate attack/release (seconds)
struct Envelope
{
    double e = 0, att = 0.5, rel = 0.99;
    void set (double fs, double attackS, double releaseS) { att = std::exp (-1 / (std::max (1e-5, attackS) * fs)); rel = std::exp (-1 / (std::max (1e-5, releaseS) * fs)); }
    double tick (double x) noexcept { const double c = x > e ? att : rel; e = c * e + (1 - c) * x; return e; }
    void reset() { e = 0; }
};

struct Noise
{
    uint32_t s = 0x9E3779B9u;
    double next() noexcept { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return double (int32_t (s)) / 2147483648.0; }
};

// band-limited LFO shapes (rate up to a few hundred Hz)
inline double lfoShape (double phase, int shape)
{
    switch (shape)
    {
        case 1: return std::sin (2 * pi * phase);                                    // sine
        case 2: return std::tanh (8 * std::sin (2 * pi * phase)) / std::tanh (8.0);  // rounded square
        case 3: return 2 * phase - 1;                                                // ramp
        default: return 1 - 4 * std::abs (phase - 0.5);                              // triangle
    }
}
}
