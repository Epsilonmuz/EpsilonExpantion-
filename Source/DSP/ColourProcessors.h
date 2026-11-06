#pragma once
// HEAT, VALVE, CRUSH and TONE: oversampled analog-style colour processors.
// All four report the same fixed latency (Oversampler::maxLatency) whatever the quality setting.
#include "Primitives.h"

namespace eps {

// ------------------------------------------------------------------ saturation curves
// u is the driven signal (1.0 ~ 0 dBFS after drive). Each returns roughly unit small-signal gain.
namespace curves {
inline double biasedTanh (double u, double b)
{
    const double t = std::tanh (b);
    return (std::tanh (u + b) - t) / (1 - t * t);
}
inline double tape (double u)      { return biasedTanh (0.85 * u, 0.06) / 0.85; }                       // smooth, almost symmetric
inline double console (double u)   { return std::atan (biasedTanh (u, 0.18) * 1.15) / 1.15; }           // transformer-ish, H2+H3
inline double germanium (double u)                                                                       // asymmetric: soft top, firmer bottom
{
    if (u >= 0) return (1 - std::exp (-1.4 * u)) / 1.4;
    const double v = -u; return -v / std::pow (1 + std::pow (v, 3.0), 1.0 / 3.0);
}
inline double triode (double u)    { return biasedTanh (1.1 * u, 0.55) / 1.1; }                          // strong even-order
inline double pentode (double u)   { const double v = 1.8 * u; return v / std::pow (1 + std::pow (std::abs (v), 2.5), 0.4) / 1.8; }   // firm knee, odd-order
inline double byStyle (int s, double u)
{
    switch (s) { case 1: return console (u); case 2: return germanium (u); case 3: return triode (u); case 4: return pentode (u); default: return tape (u); }
}
inline double valveStage1 (double u) { return biasedTanh (u, 0.32); }                                    // single-ended triode
inline double valveStage2 (double u) { return std::tanh (u) * 0.9 + 0.1 * u / (1 + u * u); }             // push-pull, odd-order
inline double crunch (double u)                                                                          // transistor output amp
{
    const double a = u >= 0 ? u : 0.8 * u;
    return std::clamp (a / std::pow (1 + std::pow (std::abs (a), 6.0), 1.0 / 6.0), -1.0, 1.0) * (u >= 0 ? 1.0 : 1.25);
}
inline double siemens (double u) { return biasedTanh (0.7 * u, 0.12) / 0.7; }
}

// Measures the steady RMS gain of a curve for a -18 dBFS sine at a given drive, for auto gain tables.
template <typename F>
inline double curveGain (F&& f, double driveGain)
{
    double in = 0, out = 0;
    for (int i = 0; i < 256; ++i)
    {
        const double x = 0.177 * std::sin (2 * pi * (i + 0.5) / 256.0);
        const double y = f (x * driveGain);
        in += x * x; out += y * y;
    }
    return std::sqrt (out / in);
}

class ColourProcessor
{
public:
    void prepare (Effect e, double fs)
    {
        effect = e; sr = fs;
        for (auto& c : ch) { c.os.prepare(); c.align.prepare (Oversampler<Lambda>::maxLatency + 4); }
        // auto-gain tables (index = drive in dB, 0..60), built with the exact drive scaling each stage uses
        for (int d = 0; d <= 60; ++d)
        {
            for (int st = 0; st < 5; ++st)
                autoTable[size_t (st)][size_t (d)] = 0.5 / std::max (1e-4, curveGain ([st] (double u) { return curves::byStyle (st, u); }, 0.5 * gain (double (d))));
            valveTable1[size_t (d)] = 1.0 / std::max (1e-4, curveGain ([] (double u) { return curves::valveStage1 (u); }, 0.35 * gain (double (d))));
            valveTable2[size_t (d)] = 0.6 * gain (double (d)) / std::max (1e-4, curveGain ([] (double u) { return curves::valveStage2 (u); }, 0.6 * gain (double (d))));
            sieTable[size_t (d)] = 1.0 / std::max (1e-4, curveGain ([] (double u) { return curves::siemens (u); }, 0.45 * gain (double (d) - 15)));
        }
        meterEnv.set (fs, 0.005, 0.3);
        crushEnv.set (fs, 0.0013, 1.7);
        reset();
    }
    void reset()
    {
        for (auto& c : ch) c.reset();
        meterEnv.reset(); crushEnv.reset();
        grSmooth = 1; counter = 0; lastStages = -1; meterValue = 0; grDb = 0; crushRel = 1.7;
    }
    static constexpr int latency() { return Oversampler<int>::maxLatency; }

    Stereo tick (Stereo x, const Values& v) noexcept
    {
        if ((counter++ & 15u) == 0) update (v);
        Stereo out;
        if (effect == Effect::Crush) crushDetector (x, v);
        for (int c = 0; c < 2; ++c)
        {
            auto& s = ch[size_t (c)];
            double in = c ? x.r : x.l;
            // ---- base-rate pre filters
            if (effect == Effect::Heat)
            {
                if (lowCutOn) in = s.lowCut.tick (in);
                if (thump) in = s.thump.tick (in);
                in = s.tilt.tick (in);
                in = s.pre.tick (in);
            }
            else if (effect == Effect::Tone)
            {
                in = s.eqLow.tick (in); in = s.eqMid.tick (in); in = s.eqHigh.tick (in);
            }
            else if (effect == Effect::Valve)
            {
                in = s.source.tick (in); in = s.presence.tick (in);
            }
            // ---- oversampled nonlinear core
            s.fn.owner = this; s.fn.c = c;
            double y = (effect == Effect::Tone && ! toneAmp) ? s.align.tick (in, latency()) : s.os.tick (in, stages, s.fn);
            if (effect != Effect::Tone || toneAmp) y = s.align.tick (y, latency() - Oversampler<Lambda>::latency (stages));
            // ---- base-rate post filters
            if (effect == Effect::Heat)
            {
                y = s.post.tick (y);
                y = s.tint.tick (y);
                if (highCutOn) y = steep ? s.highCutSteep.tick (y) : s.highCut1.lp (y, highCutA);
                y *= heatMakeup;
            }
            else if (effect == Effect::Valve) y *= valveMakeup;
            else if (effect == Effect::Crush) y = s.darkness2.tick (s.darkness1.tick (y)) * crushMakeup;
            else if (effect == Effect::Tone) y *= toneMakeup;
            y = s.dc.tick (y, dcR);
            if (effect == Effect::Valve && noisy)
            {
                y += noiseLevel * (0.8 * s.noise.next() + 0.2 * std::sin (2 * pi * s.humPhase));
                s.humPhase = wrap (s.humPhase + 60.0 / sr);
            }
            (c ? out.r : out.l) = y;
        }
        // attitude / heat meter: level going into the saturation stage, in dB
        const double drivenPeak = std::max (std::abs (x.l), std::abs (x.r)) * meterDrive;
        meterValue = meterEnv.tick (drivenPeak);
        return out;
    }
    double meter() const noexcept { return toDb (meterValue); }     // HEAT/VALVE: drive level; CRUSH: see reduction()
    double reduction() const noexcept { return grDb; }

private:
    struct Lambda
    {
        ColourProcessor* owner = nullptr;
        int c = 0;
        double operator() (double u) const noexcept { return owner->core (u, c); }
    };
    struct Channel
    {
        Oversampler<Lambda> os; Lambda fn; IntDelay align;
        Svf lowCut, thump, tilt, pre, post, tint;
        CutFilter highCutSteep; OnePole highCut1;
        Svf eqLow, eqMid, eqHigh, source, presence;
        Svf vBass, vTreble;                 // valve tone stack (oversampled rate)
        Svf darkness1, darkness2;
        DcBlock dc; Noise noise; double humPhase = 0;
        void reset()
        {
            os.reset(); align.reset();
            for (auto* f : { &lowCut, &thump, &tilt, &pre, &post, &tint, &eqLow, &eqMid, &eqHigh, &source, &presence, &vBass, &vTreble, &darkness1, &darkness2 }) f->reset();
            highCutSteep.reset(); highCut1.reset(); dc.reset(); humPhase = 0;
        }
    };

    // the nonlinear core, running at the oversampled rate
    double core (double u, int c) noexcept
    {
        switch (effect)
        {
            case Effect::Heat:  return curves::byStyle (style, u * heatDrive) / heatOut;
            case Effect::Valve:
            {
                auto& s = ch[size_t (c)];
                double y = curves::valveStage1 (u * valveIn) * valveIn1Comp;
                y = s.vTreble.tick (s.vBass.tick (y));
                return curves::valveStage2 (y * valveDrive) / valveDrive;
            }
            case Effect::Crush:
            {
                const double y = u * crushIn * grSmooth;            // level-locked signal
                return curves::crunch (y * crunchGain) / crunchNorm;   // crunchNorm keeps the locked level
            }
            case Effect::Tone:  return curves::siemens (u * toneDrive) * toneComp;
            default: return u;
        }
    }

    void crushDetector (Stereo x, const Values& v) noexcept
    {
        // Level-Loc style: ~1.3 ms attack, release slow 1.7 s / fast 0.85 s, lengthening up to ~13x when driven hard
        const double peak = std::max (std::abs (x.l), std::abs (x.r)) * crushIn;
        const double over = peak / crushTarget;
        const double baseRel = v[crush::release] > 0.5 ? 0.85 : 1.7;
        const double satur = std::clamp ((toDb (std::max (over, 1e-9)) - 6.0) / 18.0, 0.0, 1.0);
        crushRel += 0.0005 * (baseRel * (1 + 12 * satur) - crushRel);
        crushEnv.set (sr, 0.0013, crushRel);
        const double env = crushEnv.tick (peak);
        const double g = env > crushTarget ? crushTarget / env : 1.0;
        grSmooth += 0.02 * (g - grSmooth);
        grDb = -toDb (grSmooth);
    }

    void update (const Values& v) noexcept
    {
        const double fs = sr;
        switch (effect)
        {
            case Effect::Heat:
            {
                style = std::clamp (int (std::round (v[heat::style])), 0, 4);
                stages = 1 + std::clamp (int (std::round (v[heat::quality])), 0, 2);
                const double driveDb = v[heat::drive] + (v[heat::punish] > 0.5 ? 20 : 0);
                heatDrive = gain (driveDb) * 0.5;                    // 0 dB drive, 0 dBFS peak -> gentle saturation
                heatOut = 0.5;
                meterDrive = heatDrive * 2;
                // Auto: hold loudness for a -18 dBFS programme while Drive rises (as an Output knob turned against Drive)
                heatMakeup = v[heat::autogain] > 0.5 ? autoTable[size_t (style)][size_t (std::clamp (int (std::round (driveDb)), 0, 60))] : 1.0;
                thump = v[heat::thump] > 0.5; steep = v[heat::steep] > 0.5;
                lowCutOn = v[heat::lowcut] > 20.5;
                highCutOn = v[heat::highcut] < 21000;
                highCutA = OnePole::coeff (v[heat::highcut], fs);
                const double t = v[heat::tone];                     // tilt: dark (-) .. bright (+), +-6 dB at the extremes
                // style colouring: pre-emphasis into the curve, inverse after it, plus a fixed post tint
                struct StyleEq { Svf::Mode m; double f, q, db; Svf::Mode tm; double tf, tq, tdb; };
                static constexpr StyleEq eq[5] = {
                    { Svf::HighShelf, 3000, 0.6, 4.0, Svf::HighShelf, 14000, 0.7, -0.8 },   // tape: highs saturate first, soft top
                    { Svf::LowShelf, 140, 0.7, 3.5,  Svf::HighShelf, 11000, 0.6, 1.5 },     // console: beefy lows, airy top
                    { Svf::Bell, 1500, 0.8, 5.0,     Svf::LowShelf, 90, 0.7, 1.5 },          // germanium: focused mids, weighty low
                    { Svf::Bell, 700, 0.5, 1.5,      Svf::HighShelf, 9000, 0.7, -0.5 },      // triode: warm
                    { Svf::HighShelf, 2000, 0.7, 2.0, Svf::Bell, 2500, 0.9, 1.0 } };         // pentode: edgy
                const auto& e = eq[style];
                for (auto& c : ch)
                {
                    c.lowCut.set (Svf::HighPass, fs, v[heat::lowcut], 0.7071);
                    c.thump.set (Svf::Bell, fs, std::max (30.0, v[heat::lowcut] * 1.3), 1.1, 5.0);
                    c.tilt.set (Svf::HighShelf, fs, 900, 0.5, t);
                    c.pre.set (e.m, fs, e.f, e.q, e.db);
                    c.post.set (e.m, fs, e.f, e.q, -e.db);
                    c.tint.set (e.tm, fs, e.tf, e.tq, e.tdb);
                    if (steep) c.highCutSteep.set (false, 5, fs, v[heat::highcut]);
                }
                break;
            }
            case Effect::Valve:
            {
                stages = 2;
                valveIn = gain (v[valve::input]) * 0.35;
                valveIn1Comp = valveTable1[size_t (std::clamp (int (std::round (v[valve::input])), 0, 60))];
                valveDrive = gain (v[valve::drive]) * 0.6;
                {
                    const double comp2 = valveTable2[size_t (std::clamp (int (std::round (v[valve::drive])), 0, 60))];
                    // Auto keeps loudness; off, Input and Drive raise the level like the hardware's gain stages
                    valveMakeup = v[valve::autogain] > 0.5 ? comp2 : comp2 * gain (0.45 * (v[valve::input] - 8) + 0.45 * (v[valve::drive] - 4));
                }
                meterDrive = valveIn * 3;
                const bool mic = v[valve::mic] > 0.5;
                noisy = v[valve::noise] > 0.5;
                noiseLevel = gain (mic ? -78.0 : -88.0);
                const double osr = fs * 4;
                for (auto& c : ch)
                {
                    // 150-ohm mic input: lighter lows, a presence lift; 600-ohm line: flat with a soft top
                    c.source.set (Svf::HighPass, fs, mic ? 70 : 18, 0.6);
                    c.presence.set (mic ? Svf::Bell : Svf::HighShelf, fs, mic ? 4500 : 15000, mic ? 0.8 : 0.7, mic ? 2.0 : -1.0);
                    const double b = v[valve::bass], tr = v[valve::treble];
                    // cuts are wide shelves, boosts are more sloped (narrower), as on the original control
                    c.vBass.set (Svf::LowShelf, osr, b >= 0 ? 90 : 160, b >= 0 ? 0.9 : 0.5, b);
                    c.vTreble.set (Svf::HighShelf, osr, tr >= 0 ? 5000 : 3000, tr >= 0 ? 0.9 : 0.5, tr);
                }
                break;
            }
            case Effect::Crush:
            {
                stages = 2;
                crushIn = gain (v[crush::crush] * 4.0);              // 0..40 dB into the gain-reduction circuit
                crushTarget = 0.2;
                crunchGain = gain (v[crush::crunch] * 3.0);           // 0..30 dB into the output amp
                crunchNorm = std::max (1e-6, std::abs (curves::crunch (crushTarget * crunchGain))) / crushTarget;
                crushMakeup = gain (6.0);                            // locked level about -8 dBFS peak
                const double dark = 20000 * std::pow (700.0 / 20000.0, v[crush::darkness] / 10.0);
                for (auto& c : ch) { c.darkness1.set (Svf::LowPass, fs, dark, 0.54); c.darkness2.set (Svf::LowPass, fs, dark, 1.31); }
                break;
            }
            case Effect::Tone:
            {
                stages = 2;
                toneAmp = v[tone::colour] > 0.5;
                const double dr = v[tone::drive];
                toneDrive = gain (dr) * 0.45;
                // static compensation proportional to Drive: positive drive is turned back down after the amp
                toneComp = sieTable[size_t (std::clamp (int (std::round (dr + 15)), 0, 60))];
                toneMakeup = 1.0;
                static constexpr double mids[6] = { 700, 1000, 1500, 2300, 3500, 5600 };
                const double mf = mids[size_t (std::clamp (int (std::round (v[tone::midfreq])), 0, 5))];
                const double lo = v[tone::low], md = v[tone::mid], hi = v[tone::high];
                for (auto& c : ch)
                {
                    // low: cut = shelf, boost = broad bell centred very low
                    if (lo >= 0) c.eqLow.set (Svf::Bell, fs, 55, 0.55, lo); else c.eqLow.set (Svf::LowShelf, fs, 220, 0.6, lo);
                    // mid: proportional Q
                    c.eqMid.set (Svf::Bell, fs, mf, 0.45 + 0.11 * std::abs (md), md);
                    // high: cut = gentle low-pass-like shelf, boost = broad bell centred very high
                    if (hi >= 0) c.eqHigh.set (Svf::Bell, fs, std::min (16000.0, 0.42 * fs), 0.45, hi); else c.eqHigh.set (Svf::HighShelf, fs, 4500, 0.5, hi);
                }
                break;
            }
            default: break;
        }
        dcR = std::exp (-2 * pi * 6 / fs);
        if (stages != lastStages) { lastStages = stages; for (auto& c : ch) c.os.reset(); }
    }

    Effect effect = Effect::Heat;
    double sr = 48000, dcR = 0.999;
    unsigned counter = 0;
    int stages = 2, lastStages = -1, style = 0;
    std::array<Channel, 2> ch;
    std::array<std::array<double, 61>, 5> autoTable {};
    std::array<double, 61> valveTable1 {}, valveTable2 {}, sieTable {};
    // heat
    double heatDrive = 1, heatOut = 1, heatMakeup = 1, highCutA = 1; bool thump = false, steep = false, highCutOn = false, lowCutOn = false;
    // valve
    double valveIn = 1, valveIn1Comp = 1, valveDrive = 1, valveMakeup = 1, noiseLevel = 0; bool noisy = false;
    // crush
    double crushIn = 1, crushTarget = 0.2, crunchGain = 1, crunchNorm = 1, crushMakeup = 1, grSmooth = 1, grDb = 0, crushRel = 1.7;
    Envelope crushEnv;
    // tone
    double toneDrive = 1, toneComp = 1, toneMakeup = 1; bool toneAmp = true;
    // meters
    Envelope meterEnv; double meterDrive = 1, meterValue = 0;
};
}
