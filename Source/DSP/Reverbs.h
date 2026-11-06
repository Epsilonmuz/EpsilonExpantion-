#pragma once
// PLATE (plate-class tank after J. Dattorro, "Effect Design Part 1", JAES 1997) and
// SPACE (a swarm of envelope-shaped taps across a "time tunnel", with diffusion and colour over time).
#include "ColourProcessors.h"

namespace eps {

// Schroeder all-pass on a Ring, with optional modulated length; internal taps readable for outputs.
struct AllPass
{
    Ring line; double len = 100;
    void prepare (double maxLen) { line.prepare (size_t (maxLen) + 8); }
    void reset() { line.reset(); }
    double tick (double x, double g, double mod = 0) noexcept
    {
        const double d = mod == 0 ? line.age (len - 1) : line.ageCubic (len - 1 + mod);
        const double y = d - g * x;
        line.push (x + g * y);
        return y;
    }
    double tap (double t) const noexcept { return line.age (t); }
};

// ================================================================== PLATE
class PlateProcessor
{
public:
    void prepare (double fs)
    {
        sr = fs;
        const double k = fs / 29761.0 * 1.25;                 // room for the largest model
        for (int i = 0; i < 4; ++i) inAp[size_t (i)].prepare (400 * k);
        for (auto& c : disp) for (auto& a : c) a.prepare (0.02 * fs);
        for (auto& a : tankAp1) a.prepare ((908 + 40) * k);
        for (auto& a : tankAp2) a.prepare (2656 * k);
        for (auto& d : tankD1) d.prepare (size_t (4453 * k) + 8);
        for (auto& d : tankD2) d.prepare (size_t (3720 * k) + 8);
        for (auto& p : pre) p.prepare (size_t (0.26 * fs) + 8);
        autoEnv.set (fs, 0.005, 0.35);
        reset();
    }
    void reset()
    {
        for (auto& a : inAp) a.reset();
        for (auto& c : disp) for (auto& a : c) a.reset();
        for (auto& a : tankAp1) a.reset(); for (auto& a : tankAp2) a.reset();
        for (auto& d : tankD1) d.reset(); for (auto& d : tankD2) d.reset();
        for (auto& p : pre) p.reset();
        for (auto& f : bw) f.reset(); for (auto& f : damp) f.reset();
        for (auto& f : outLo) f.reset(); for (auto& f : outHi) f.reset(); for (auto& f : eq1) f.reset(); for (auto& f : eq2) f.reset();
        cross = {}; phase = 0; phase2 = 0; counter = 0; autoEnv.reset(); decayG = 0.5; freezeMix = 0;
    }
    Stereo tick (Stereo x, const Values& v) noexcept
    {
        if ((counter++ & 31u) == 0) update (v);
        const bool freeze = v[plate::freeze] > 0.5;
        freezeMix += ((freeze ? 1.0 : 0.0) - freezeMix) * 0.0008;
        // input amp
        const int amp = int (std::round (v[plate::amp]));
        auto drive = [amp] (double s) { return amp == 1 ? curves::biasedTanh (s * 1.4, 0.2) / 1.4 : amp == 2 ? s / std::cbrt (1 + std::pow (std::abs (s * 1.6), 3.0)) : s; };
        Stereo in { drive (x.l), drive (x.r) };
        pre[0].push (in.l); pre[1].push (in.r);
        const double pd = v[plate::predelay] * 0.001 * sr;
        // auto-decay: loud input shortens the tail, which recovers as the input falls
        const double env = autoEnv.tick (std::max (std::abs (x.l), std::abs (x.r)));
        const double squeeze = v[plate::autodecay] * std::clamp ((toDb (env) + 40.0) / 20.0, 0.0, 1.0);
        const double g = decayFor (rt60 * (1 - 0.85 * squeeze));
        const double gTank = g + freezeMix * (1 - g);
        const double modDepth = v[plate::mod] * 12.0 * scale;
        const double lfo = std::sin (2 * pi * phase), lfo2 = std::cos (2 * pi * phase);
        const double lfo3 = std::sin (2 * pi * phase2), lfo4 = std::sin (2 * pi * (phase2 * 1.37 + 0.31));
        phase = wrap (phase + v[plate::rate] / sr);
        phase2 = wrap (phase2 + 0.137 / sr);
        const double wander = (2.0 + 6.0 * v[plate::mod]) * scale * sr / 29761.0;

        // input diffusion (per side), then into the two halves of the tank
        std::array<double, 2> d {};
        for (int c = 0; c < 2; ++c)
        {
            double s = pre[size_t (c)].age (pd) * (1 - freezeMix);
            s = bw[size_t (c)].lp (s, bwA);
            // plate dispersion: high frequencies travel faster in the plate, smearing transients into a dense burst
            for (auto& a : disp[size_t (c)]) s = a.tick (s, dispG);
            if (c == 0) { s = inAp[0].tick (s, id1); s = inAp[1].tick (s, id1); }
            else        { s = inAp[2].tick (s, id2); s = inAp[3].tick (s, id2); }
            d[size_t (c)] = s;
        }
        std::array<double, 2> next {};
        for (int c = 0; c < 2; ++c)
        {
            double s = d[size_t (c)] + cross[size_t (1 - c)];
            s = tankAp1[size_t (c)].tick (s, -dd1, modDepth * (c ? lfo2 : lfo));
            tankD1[size_t (c)].push (s);
            // slow, small wander of the long tank delays breaks up the fixed modes that make plates ring
            s = tankD1[size_t (c)].ageCubic (len1[size_t (c)] - 1 + wander * (c ? lfo4 : lfo3));
            s = damp[size_t (c)].lp (s, freeze ? 1.0 : dampA) * gTank;
            s = tankAp2[size_t (c)].tick (s, dd2);
            tankD2[size_t (c)].push (s);
            next[size_t (c)] = tankD2[size_t (c)].age (len2[size_t (c)] - 1) * gTank;
        }
        cross = next;
        // output taps (Dattorro's tap set, scaled)
        auto T = [this] (double t) { return t * scale * sr / 29761.0; };
        double L = 0.6 * (tankD1[1].age (T (266)) + tankD1[1].age (T (2974)) - tankAp2[1].tap (T (1913)) + tankD2[1].age (T (1996))
                        - tankD1[0].age (T (1990)) - tankAp2[0].tap (T (187)) - tankD2[0].age (T (1066)));
        double R = 0.6 * (tankD1[0].age (T (353)) + tankD1[0].age (T (3627)) - tankAp2[0].tap (T (1228)) + tankD2[0].age (T (2673))
                        - tankD1[1].age (T (2111)) - tankAp2[1].tap (T (335)) - tankD2[1].age (T (121)));
        // model voicing, then wet filters and width
        L = eq2[0].tick (eq1[0].tick (L)); R = eq2[1].tick (eq1[1].tick (R));
        L = outHi[0].tick (outLo[0].tick (L)); R = outHi[1].tick (outLo[1].tick (R));
        const double m = 0.5 * (L + R), sd = 0.5 * (L - R) * v[plate::width];
        return { m + sd, m - sd };
    }
private:
    double decayFor (double rt) const noexcept
    {
        // the signal passes 4 decay gains per round trip of both tank halves
        const double loop = (len1[0] + len2[0] + len1[1] + len2[1] + apLen) / sr;
        return std::pow (10.0, -3.0 * loop / (4.0 * std::max (0.05, rt)));
    }
    void update (const Values& v) noexcept
    {
        struct Model { double size, id1, id2, dd1, dd2, bwHz; Svf::Mode m1; double f1, q1, g1; Svf::Mode m2; double f2, q2, g2; double disp; };
        static constexpr Model models[5] = {
            { 1.00, 0.75, 0.625, 0.70, 0.50, 9000,  Svf::LowShelf, 200, 0.7, 0.0,   Svf::HighShelf, 8000, 0.7, 0.0, 0.62 },   // steel: big, floaty
            { 0.62, 0.70, 0.600, 0.65, 0.45, 12000, Svf::LowShelf, 220, 0.7, -3.0,  Svf::Bell, 1200, 0.9, 2.0, 0.55 },        // gold foil: compact, boxier
            { 0.88, 0.75, 0.650, 0.72, 0.55, 14000, Svf::LowShelf, 200, 0.7, -2.0,  Svf::Bell, 3500, 0.8, 3.0, 0.6 },        // bright: upper-mid edge
            { 0.95, 0.78, 0.660, 0.70, 0.50, 6500,  Svf::LowShelf, 250, 0.7, 1.5,   Svf::HighShelf, 6000, 0.7, -3.0, 0.66 },  // warm: solid, darker
            { 0.75, 0.60, 0.500, 0.60, 0.45, 16000, Svf::LowShelf, 200, 0.7, -1.0,  Svf::HighShelf, 7000, 0.7, 3.0, 0.5 } }; // splash: bright, splashy
        const int mi = std::clamp (int (std::round (v[plate::model])), 0, 4);
        const auto& md = models[mi];
        scale = md.size;
        const double k = sr / 29761.0 * scale;
        static constexpr double inLen[4] = { 142, 107, 379, 277 };
        static constexpr double dl[8] = { 0.0013, 0.0019, 0.0027, 0.0037, 0.0051, 0.0067, 0.0089, 0.0113 };
        for (int c = 0; c < 2; ++c) for (int i = 0; i < 8; ++i) disp[size_t (c)][size_t (i)].len = dl[i] * sr * (0.8 + 0.2 * scale) * (c ? 1.09 : 1.0);
        dispG = md.disp;
        for (int i = 0; i < 4; ++i) inAp[size_t (i)].len = inLen[i] * k;
        tankAp1[0].len = 672 * k; tankAp1[1].len = 908 * k;
        tankAp2[0].len = 1800 * k; tankAp2[1].len = 2656 * k;
        len1 = { 4453 * k, 4217 * k }; len2 = { 3720 * k, 3163 * k };
        apLen = (672 + 908 + 1800 + 2656) * k;
        id1 = md.id1; id2 = md.id2; dd1 = md.dd1; dd2 = md.dd2;
        bwA = OnePole::coeff (md.bwHz, sr);
        dampA = OnePole::coeff (v[plate::damping], sr);
        rt60 = v[plate::decay];
        for (int c = 0; c < 2; ++c)
        {
            eq1[size_t (c)].set (md.m1, sr, md.f1, md.q1, md.g1);
            eq2[size_t (c)].set (md.m2, sr, md.f2, md.q2, md.g2);
            outLo[size_t (c)].set (Svf::HighPass, sr, v[plate::lowcut], 0.7071);
            outHi[size_t (c)].set (Svf::LowPass, sr, std::min (v[plate::highcut], 0.45 * sr), 0.7071);
        }
    }
    double sr = 48000, scale = 1, phase = 0, phase2 = 0, rt60 = 2.5, decayG = 0.5, freezeMix = 0;
    double id1 = 0.75, id2 = 0.625, dd1 = 0.7, dd2 = 0.5, bwA = 0.5, dampA = 0.5, apLen = 0;
    unsigned counter = 0;
    std::array<AllPass, 4> inAp;
    std::array<std::array<AllPass, 8>, 2> disp;
    double dispG = 0.6;
    std::array<AllPass, 2> tankAp1, tankAp2;
    std::array<Ring, 2> tankD1, tankD2, pre;
    std::array<double, 2> len1 {}, len2 {}, cross {};
    std::array<OnePole, 2> bw, damp;
    std::array<Svf, 2> outLo, outHi, eq1, eq2;
    Envelope autoEnv;
};

// ================================================================== SPACE
class SpaceProcessor
{
    static constexpr int taps = 64, groups = 4;
public:
    void prepare (double fs)
    {
        sr = fs;
        const double maxSeconds = 62.0 * std::min (1.0, 96000.0 / fs);
        maxTime = maxSeconds - 1.5;
        for (auto& b : buffer) b.prepare (size_t (maxSeconds * fs));
        for (int c = 0; c < 2; ++c)
        {
            for (int i = 0; i < 6; ++i) diff[size_t (c)][size_t (i)].prepare (0.05 * fs);
            for (int i = 0; i < 4; ++i) smear[size_t (c)][size_t (i)].prepare (0.25 * fs);
        }
        // fixed per-tap jitter and modulation phases (deterministic)
        Noise n; n.s = 0x5EEDu;
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < taps; ++i) { jitter[size_t (c)][size_t (i)] = n.next(); modPhase[size_t (c)][size_t (i)] = 0.5 + 0.5 * n.next(); }
        reset();
    }
    void reset()
    {
        for (auto& b : buffer) b.reset();
        for (auto& c : diff) for (auto& a : c) a.reset();
        for (auto& c : smear) for (auto& a : c) a.reset();
        for (auto& c : tilt) for (auto& f : c) f.reset();
        curTime = -1; frozen = false; frozenTau = 0; dip = 1; pendingTime = -1; counter = 0; phase = 0;
    }
    Stereo tick (Stereo x, const Values& v, double bpm) noexcept
    {
        if ((counter++ & 63u) == 0) update (v, bpm);
        const bool freeze = v[space::freeze] > 0.5;
        if (freeze && ! frozen) { frozen = true; frozenTau = 0; frozenT = curTime; }
        if (! freeze && frozen) frozen = false;

        // time changes: Warp glides (pitch moves like tape); otherwise a short dip and a jump
        if (curTime < 0) curTime = targetTime;
        if (v[space::warp] > 0.5) curTime += (targetTime - curTime) * 0.00012;
        else if (std::abs (targetTime - curTime) > 0.01 * curTime)
        {
            dip = std::max (0.0, dip - 1.0 / (0.03 * sr));
            if (dip <= 0) curTime = targetTime;
        }
        else dip = std::min (1.0, dip + 1.0 / (0.05 * sr));

        // diffuse the input (Texture), then record
        for (int c = 0; c < 2; ++c)
        {
            double s = c ? x.r : x.l;
            for (int i = 0; i < 6; ++i) s = diff[size_t (c)][size_t (i)].tick (s, diffG);
            if (! frozen) buffer[size_t (c)].push (s);
        }
        const double T = (frozen ? frozenT : curTime) * sr;
        const double modDepth = v[space::mod] * 0.006 * sr;
        phase = wrap (phase + 0.23 / sr);
        Stereo out;
        for (int c = 0; c < 2; ++c)
        {
            std::array<double, groups> g {};
            if (modTick == 0)                                            // per-tap chorus offsets at control rate
                for (int i = 0; i < taps; ++i)
                    modOffset[size_t (c)][size_t (i)] = modDepth * std::sin (2 * pi * (phase * modPhase[size_t (c)][size_t (i)] * 3 + modPhase[size_t (c)][size_t (i)]));
            for (int i = 0; i < taps; ++i)
            {
                const double pos = position[size_t (c)][size_t (i)];
                double age = pos * T + modOffset[size_t (c)][size_t (i)];
                if (frozen)
                {
                    // the last T seconds before Freeze repeat as an endless loop
                    double t = std::fmod (frozenTau - pos * T, T); if (t < 0) t += T;
                    age = T - t;
                }
                g[size_t (std::min (groups - 1, int (pos * groups)))] += weight[size_t (i)] * buffer[size_t (c)].age (std::max (0.0, age));
            }
            double s = 0;
            for (int k = 0; k < groups; ++k) s += tilt[size_t (c)][size_t (k)].tick (g[size_t (k)]);
            for (int i = 0; i < 4; ++i) s = smear[size_t (c)][size_t (i)].tick (s, smearG);
            (c ? out.r : out.l) = s * dip;
        }
        if (frozen) frozenTau += 1;
        modTick = (modTick + 1) & 31;
        return out;
    }
    double windowSeconds() const noexcept { return curTime; }
    // envelope (0..1) at a position in the window, for the editor's display
    static double envelopeAt (double p, double shape, double contour)
    {
        const double decay = std::exp (-4.5 * p);
        const double bloom = std::pow (std::sin (pi * std::clamp (p, 0.0, 1.0)), 1.4);
        const double rev = std::exp (-4.5 * (1 - p)) * (p < 0.96 ? 1.0 : (1 - p) / 0.04);
        const double e = shape < 0.5 ? decay + (bloom - decay) * shape * 2 : bloom + (rev - bloom) * (shape - 0.5) * 2;
        return (1 - contour) + contour * e;
    }
private:
    void update (const Values& v, double bpm) noexcept
    {
        static constexpr double beatCounts[6] = { 1, 2, 4, 8, 16, 32 };
        double t = v[space::sync] > 0.5 ? 60.0 / bpm * beatCounts[size_t (std::clamp (int (std::round (v[space::beats])), 0, 5))] : v[space::time];
        targetTime = std::clamp (t, 0.1, maxTime);
        const double tex = v[space::texture];
        diffG = 0.72 * tex;
        smearG = 0.55 * tex * tex;
        const double kd = (0.4 + 0.6 * tex) * sr;
        static constexpr double dl[6] = { 0.0031, 0.0047, 0.0071, 0.0113, 0.0173, 0.0241 }, sl[4] = { 0.037, 0.061, 0.097, 0.149 };
        for (int c = 0; c < 2; ++c)
        {
            for (int i = 0; i < 6; ++i) diff[size_t (c)][size_t (i)].len = dl[i] * kd * (c ? 1.07 : 1.0);
            for (int i = 0; i < 4; ++i) smear[size_t (c)][size_t (i)].len = sl[i] * (0.3 + 0.7 * std::min (1.0, targetTime / 4.0)) * sr * (c ? 1.05 : 1.0);
            // tap positions: regular grid, jittered more as Texture rises
            for (int i = 0; i < taps; ++i)
                position[size_t (c)][size_t (i)] = std::clamp ((i + 0.5 + 0.45 * tex * jitter[size_t (c)][size_t (i)]) / taps, 0.0, 1.0);
            // colour tilts later taps more: a high shelf whose gain grows across the window
            for (int k = 0; k < groups; ++k)
                tilt[size_t (c)][size_t (k)].set (Svf::HighShelf, sr, 2500, 0.6, v[space::colour] * 9.0 * (k + 0.5) / groups);
        }
        double sumSq = 0;
        for (int i = 0; i < taps; ++i)
        {
            const double w = envelopeAt ((i + 0.5) / taps, v[space::shape], v[space::contour]);
            weight[size_t (i)] = w; sumSq += w * w;
        }
        const double norm = 1.0 / std::sqrt (std::max (1e-9, sumSq));
        for (auto& w : weight) w *= norm;
    }
    double sr = 48000, maxTime = 60, targetTime = 4, curTime = -1, frozenT = 4, frozenTau = 0, dip = 1, pendingTime = -1, phase = 0;
    double diffG = 0.5, smearG = 0.3;
    bool frozen = false;
    unsigned counter = 0;
    std::array<Ring, 2> buffer;
    std::array<std::array<AllPass, 6>, 2> diff;
    std::array<std::array<AllPass, 4>, 2> smear;
    std::array<std::array<Svf, groups>, 2> tilt;
    std::array<std::array<double, taps>, 2> position {}, jitter {}, modPhase {}, modOffset {};
    int modTick = 0;
    std::array<double, taps> weight {};
};
}
