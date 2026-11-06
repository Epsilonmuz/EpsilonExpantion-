#pragma once
// PRISM (splice / reverse / pitch echo) and TIME (dual variable-clock delay).
#include "Primitives.h"

namespace eps {

// ================================================================== PRISM
// The input (plus recycled output) is recorded continuously. Every splice period a new splice starts:
// it plays the most recent stretch of audio (behind Delay) forward or backward at the pitch ratio,
// at constant speed, so notes inside a splice are exactly in tune. Consecutive splices cross-fade
// over the Smoothing time. Splices are sequential (two voices at most), not overlapping grains.
class PrismProcessor
{
    struct Voice
    {
        bool active = false;
        double age = 0, length = 1, fade = 1, start = 0, speed = 1;   // start: absolute buffer time (samples)
        bool reverse = false;
    };
public:
    void prepare (double fs)
    {
        sr = fs;
        for (auto& b : buffer) b.prepare (size_t (fs * 10.5) + 64);
        gateEnv.set (fs, 0.004, 0.18);
        reset();
    }
    void reset()
    {
        for (auto& b : buffer) b.reset();
        for (auto& c : voices) for (auto& v : c) v = {};
        clock = 0; nextSplice = { 0, 0 }; fb = {}; gateEnv.reset(); gateGain = 1;
        for (auto& f : hp) f.reset(); for (auto& f : lp) f.reset();
        counter = 0;
    }
    Stereo tick (Stereo x, const Values& v, double bpm) noexcept
    {
        if ((counter++ & 31u) == 0) update (v);
        // record input + recycle
        const double rec = v[prism::recycle];
        Stereo feed = fb;
        const int mode = int (std::round (v[prism::fbmode]));
        if (mode == 0) { const double m = 0.5 * (fb.l + fb.r); feed = { m, m }; }
        else if (mode == 2) feed = { fb.r, fb.l };
        buffer[0].push (x.l + rec * feed.l);
        buffer[1].push (x.r + rec * feed.r);

        const double spliceMs = v[prism::sync] > 0.5 ? 60000.0 / bpm * beats (v[prism::note]) : v[prism::splice];
        const double delay = v[prism::delay] * 0.001 * sr;
        const double ratioBase = std::exp2 (v[prism::pitch] / 12.0);
        const double spread = v[prism::spread] / 1200.0;          // cents -> octaves, half per side
        const bool rev = v[prism::reverse] > 0.5;
        Stereo wet;
        for (int c = 0; c < 2; ++c)
        {
            double len = std::max (0.02 * sr, spliceMs * 0.001 * sr);
            if (c == 1) len *= 1.0 - v[prism::offset];
            const double ratio = ratioBase * std::exp2 ((c ? 0.5 : -0.5) * spread);
            const double fadeLen = std::max (0.003 * sr, v[prism::smoothing] * 0.5 * len);
            if (double (clock) >= nextSplice[size_t (c)])
            {
                // start a splice on the free voice
                auto& slot = voices[size_t (c)][voices[size_t (c)][0].active ? 1 : 0];
                if (voices[size_t (c)][0].active && voices[size_t (c)][1].active) voices[size_t (c)][0].active = false;
                slot.active = true; slot.age = 0; slot.length = len + fadeLen; slot.fade = fadeLen;
                slot.speed = ratio; slot.reverse = rev;
                const double span = ratio * slot.length;              // source audio this splice consumes
                const double end = double (clock) - delay - 2;        // newest usable sample
                slot.start = rev ? end : end - span;
                nextSplice[size_t (c)] = double (clock) + len;
            }
            double sum = 0;
            for (auto& vo : voices[size_t (c)])
            {
                if (! vo.active) continue;
                const double pos = vo.reverse ? vo.start - vo.speed * vo.age : vo.start + vo.speed * vo.age;
                const double ago = double (clock) - pos;
                double w = 1;
                if (vo.age < vo.fade) w = 0.5 - 0.5 * std::cos (pi * vo.age / vo.fade);
                else if (vo.age > vo.length - vo.fade) w = 0.5 - 0.5 * std::cos (pi * (vo.length - vo.age) / vo.fade);
                sum += w * buffer[size_t (c)].ageCubic (std::max (1.0, ago));
                vo.age += 1;
                if (vo.age >= vo.length) vo.active = false;
            }
            sum = lp[size_t (c)].tick (hp[size_t (c)].tick (sum));
            (c ? wet.r : wet.l) = sum;
        }
        // bounded recycle (soft limit keeps high Recycle musical instead of exploding)
        fb = { std::tanh (wet.l), std::tanh (wet.r) };
        // gate / duck on the wet signal, keyed by the input level
        const double env = gateEnv.tick (std::max (std::abs (x.l), std::abs (x.r)));
        const double above = env > gain (v[prism::threshold]) ? 1.0 : 0.0;
        const double gd = v[prism::gateduck];
        const double target = gd >= 0 ? 1 - gd * above : 1 - (-gd) * (1 - above);
        gateGain += (target - gateGain) * (target < gateGain ? 0.004 : 0.0015);
        ++clock;
        return { wet.l * gateGain, wet.r * gateGain };
    }
private:
    void update (const Values& v) noexcept
    {
        for (auto& f : hp) f.set (Svf::HighPass, sr, v[prism::lowcut], 0.7071);
        for (auto& f : lp) f.set (Svf::LowPass, sr, std::min (v[prism::highcut], 0.45 * sr), 0.7071);
    }
    double sr = 48000, gateGain = 1;
    uint64_t clock = 0;
    unsigned counter = 0;
    std::array<double, 2> nextSplice {};
    std::array<Ring, 2> buffer;
    std::array<std::array<Voice, 2>, 2> voices {};
    std::array<Svf, 2> hp, lp;
    Stereo fb;
    Envelope gateEnv;
};

// ================================================================== TIME
// Two delay lines (A and B) recorded through a variable clock: Multiply 1x/2x/4x/8x lengthens the
// delay by that factor and divides the bandwidth (12 / 6 / 3 / 1.5 kHz), like a clocked digital delay.
// Each line has its own time, feedback and level; Mode routes them (Classic, Ping-Pong, Series, Cross).
class TimeProcessor
{
    struct Line
    {
        Ring mem;                          // storage at the reduced clock
        double held = 0, acc = 0, cur = -1, last = 0;
        Svf aaIn1, aaIn2, recon1, recon2;  // anti-alias / reconstruction (4th order)
        Svf fbHp, fbLp;
        // freeze
        bool frozen = false; double loopLen = 1, loopPos = 0;
        void reset()
        {
            mem.reset(); held = acc = last = 0; cur = -1; frozen = false; loopPos = 0;
            for (auto* f : { &aaIn1, &aaIn2, &recon1, &recon2, &fbHp, &fbLp }) f->reset();
        }
    };
public:
    void prepare (double fs)
    {
        sr = fs;
        for (auto& l : line) l.mem.prepare (size_t (fs * 5.2) + 64);    // 2x the longest delay, so Freeze can cross-fade its seam
        reset();
    }
    void reset() { for (auto& l : line) l.reset(); phase = 0; counter = 0; lastMult = -1; }

    Stereo tick (Stereo x, const Values& v, double bpm) noexcept
    {
        if ((counter++ & 31u) == 0) update (v);
        const int mode = int (std::round (v[time::mode]));
        const double mono = 0.5 * (x.l + x.r);
        const double lfo = lfoShape (phase, int (std::round (v[time::shape])));
        phase = wrap (phase + v[time::rate] / sr);
        const bool freeze = v[time::freeze] > 0.5;

        // targets in samples at the full clock
        double tA = v[time::sync] > 0.5 ? 60.0 / bpm * beats (v[time::noteA]) * 1000 : v[time::timeA];
        double tB = v[time::link] > 0.5 ? tA * 1.5 : (v[time::sync] > 0.5 ? 60.0 / bpm * beats (v[time::noteB]) * 1000 : v[time::timeB]);
        tA = std::min (tA, 2500.0); tB = std::min (tB, 2500.0);

        // routing per mode (A plays left, B plays right)
        const double a = line[0].last, b = line[1].last;
        double inA, inB;
        switch (mode)
        {
            case 1:  inA = mono + b * fbA; inB = a; break;                 // ping-pong: A -> B -> A, loop gain = Feedback A
            case 2:  inA = mono + b * fbA; inB = a + b * fbB * 0.5; break; // series: A then B, B recirculates to A
            case 3:  inA = x.l + b * fbA; inB = x.r + a * fbB; break;      // cross: each line feeds the other
            default: inA = x.l + a * fbA; inB = x.r + b * fbB; break;      // classic: two independent lines
        }
        const double outA = runLine (line[0], inA, tA, lfo, v, freeze);
        const double outB = runLine (line[1], inB, tB, -lfo, v, freeze);
        return { v[time::levelA] * outA, v[time::levelB] * outB };
    }
    int multiply() const noexcept { return mult; }
private:
    double runLine (Line& l, double in, double ms, double lfo, const Values& v, bool freeze) noexcept
    {
        // feedback-path tone and soft saturation (bounded, so Feedback above 100 % sings but cannot explode)
        double wIn = l.fbLp.tick (l.fbHp.tick (in));
        wIn = std::tanh (wIn * 0.8) / 0.8;
        // anti-alias at the current clock, then sample-and-hold at fs / mult
        wIn = l.aaIn2.tick (l.aaIn1.tick (wIn));
        const double clockSamples = ms * 0.001 * sr;                        // stored samples; at mult x the clock is mult x slower
        const double modAmt = v[time::depth] * std::min (0.2 * clockSamples, 0.010 * sr);
        const double target = std::max (2.0, clockSamples + modAmt * lfo);
        if (l.cur < 0) l.cur = target;
        l.cur += (target - l.cur) * 0.0006;                                   // tape-like glide when Time moves
        if (freeze && ! l.frozen) { l.frozen = true; l.loopLen = std::max (16.0, std::round (l.cur)); l.loopPos = 0; }
        if (! freeze) l.frozen = false;
        l.acc += 1.0;
        if (l.acc >= mult)
        {
            l.acc -= mult;
            if (l.frozen)
            {
                // loop the last loopLen stored samples (oldest -> newest); near the seam, fade into the
                // audio that preceded the loop so the wrap is continuous
                const double L = l.loopLen, p = l.loopPos;
                double y = l.mem.age (L - p);
                const double xf = std::min (L * 0.1, 0.006 * sr / mult);
                if (p > L - xf)
                {
                    const double w = 0.5 - 0.5 * std::cos (pi * (p - (L - xf)) / xf);
                    y += w * (l.mem.age (2 * L - p) - y);
                }
                l.held = y;
                l.loopPos += 1; if (l.loopPos >= L) l.loopPos -= L;
            }
            else
            {
                l.held = l.mem.age (l.cur - 1);
                // 12-bit storage, as on early clocked delays
                const double q = std::round (std::clamp (wIn, -2.0, 2.0) * 2048.0) / 2048.0;
                l.mem.push (q);
            }
        }
        const double y = l.recon2.tick (l.recon1.tick (l.held));
        l.last = y;
        return y;
    }
    void update (const Values& v) noexcept
    {
        mult = 1 << std::clamp (int (std::round (v[time::multiply])), 0, 3);
        const double bw = std::min (12000.0 / mult, 0.45 * sr);
        fbA = v[time::feedbackA]; fbB = v[time::feedbackB];
        for (auto& l : line)
        {
            l.aaIn1.set (Svf::LowPass, sr, bw, 0.5412); l.aaIn2.set (Svf::LowPass, sr, bw, 1.3066);
            l.recon1.set (Svf::LowPass, sr, bw, 0.5412); l.recon2.set (Svf::LowPass, sr, bw, 1.3066);
            l.fbHp.set (Svf::HighPass, sr, v[time::lowcut], 0.7071);
            l.fbLp.set (Svf::LowPass, sr, std::min (v[time::highcut], 0.45 * sr), 0.7071);
        }
        if (mult != lastMult) { lastMult = mult; for (auto& l : line) l.cur = -1; }
    }
    double sr = 48000, phase = 0, fbA = 0.3, fbB = 0.3;
    int mult = 1, lastMult = -1;
    unsigned counter = 0;
    std::array<Line, 2> line;
};
}
