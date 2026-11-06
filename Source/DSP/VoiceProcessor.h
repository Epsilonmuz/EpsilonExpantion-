#pragma once
// VOICE: pitch and formant processor with Transpose, Quantize and Robot modes.
//
// v0.3: low-latency engine. The earlier version used a general-purpose phase vocoder
// (Signalsmith Stretch) with a 2048-sample analysis block: about 46-50 ms of latency and
// 18-30 % of a core. A singing or speaking voice is monophonic and periodic, so this version
// uses TD-PSOLA (pitch-synchronous overlap-add, Moulines & Charpentier 1990):
//   * a YIN tracker finds the period of the voice;
//   * two-period Hann grains are cut from the input one period apart (analysis marks);
//   * they are laid down again at the new period (synthesis marks) -> new pitch.
// Each grain keeps the vocal tract's response, so formants stay put by themselves; reading a
// grain faster or slower moves the formants independently (Formant knob, or Link = both).
// Grains are read while they play, so the look-ahead only has to cover half a period:
// latency is 10 ms plus the drive stage's oversampler, and CPU is a few percent.
#include "ColourProcessors.h"

namespace eps {

// Streaming YIN (de Cheveigne & Kawahara 2002) on a mono signal decimated to about 16 kHz.
class PitchTracker
{
public:
    void prepare (double fs)
    {
        decim = std::max (1, int (std::lround (fs / 16000.0)));  // analyse at about 16 kHz whatever the host rate
        rate = fs / decim;
        aa1.set (Svf::LowPass, fs, 0.4 * rate, 0.5412); aa2.set (Svf::LowPass, fs, 0.4 * rate, 1.3066);
        window = int (std::ceil (0.020 * rate));
        hop = std::max (16, int (0.004 * rate));               // a new estimate every 4 ms
        absMaxTau = int (rate / 50.0);
        hist.assign (size_t (window + 2 * absMaxTau + 4), 0.0f);
        diffBuf.assign (size_t (absMaxTau + 2), 0.0);
        diff.assign (size_t (absMaxTau + 2), 0.0);
        setRange (65.0, 1100.0);
        reset();
    }
    void setRange (double loHz, double hiHz) noexcept
    {
        maxTau = std::min (absMaxTau, int (rate / loHz));
        minTau = std::max (2, int (rate / hiHz));
    }
    void reset() { std::fill (hist.begin(), hist.end(), 0.0f); std::fill (diff.begin(), diff.end(), 0.0); pos = 0; phase = 0; sinceRun = 0; sinceExact = 0; f0 = 0; confidence = 0; aa1.reset(); aa2.reset(); }
    // feed one sample at the full rate; returns true when a new estimate is available
    bool push (double x) noexcept
    {
        const double y = aa2.tick (aa1.tick (x));                     // anti-alias for the decimation
        if (int (phase++ % unsigned (decim)) != 0) return false;
        hist[size_t (pos)] = float (y); pos = (pos + 1) % int (hist.size());
        // slide the difference function by one sample: add the newest term, drop the oldest
        // (O(maxTau) per sample instead of O(window * maxTau) per estimate)
        const double x0 = at (0), xo = at (window);
        for (int t = 1; t <= absMaxTau; ++t)
        {
            const double a = x0 - at (t), b = xo - at (window + t);
            diff[size_t (t)] += a * a - b * b;
        }
        if (++sinceRun < hop) return false;
        sinceRun = 0;
        if (++sinceExact >= 50) { sinceExact = 0; recompute(); }   // clear rounding drift every 200 ms
        estimate(); return true;
    }
    double frequency() const noexcept { return f0; }
    double voiced() const noexcept { return confidence; }
private:
    float at (int ago) const noexcept { const int n = int (hist.size()); return hist[size_t ((pos - 1 - ago + 2 * n) % n)]; }
    void recompute() noexcept
    {
        for (int t = 1; t <= absMaxTau; ++t)
        {
            double d = 0;
            for (int i = 0; i < window; ++i) { const double q = double (at (i)) - at (i + t); d += q * q; }
            diff[size_t (t)] = d;
        }
    }
    void estimate() noexcept
    {
        double energy = 0;
        for (int i = 0; i < window; ++i) energy += double (at (i)) * at (i);
        if (energy < window * 1e-7) { confidence = 0; return; }
        double running = 0;
        int best = -1; double bestVal = 1;
        for (int t = 1; t <= maxTau; ++t)
        {
            const double d = std::max (0.0, diff[size_t (t)]);
            running += d;
            diffBuf[size_t (t)] = d * t / std::max (1e-12, running);
        }
        for (int t = minTau; t < maxTau; ++t)
            if (diffBuf[size_t (t)] < 0.15) { while (t + 1 < maxTau && diffBuf[size_t (t + 1)] < diffBuf[size_t (t)]) ++t; best = t; bestVal = diffBuf[size_t (t)]; break; }
        if (best < 0) { confidence = 0; return; }
        const double a = diffBuf[size_t (best - 1)], b = diffBuf[size_t (best)], c = diffBuf[size_t (best + 1)];
        const double den = a - 2 * b + c;
        const double off = std::abs (den) > 1e-12 ? 0.5 * (a - c) / den : 0.0;
        f0 = rate / (best + std::clamp (off, -1.0, 1.0));
        confidence = 1 - bestVal;
    }
    std::vector<float> hist; std::vector<double> diffBuf, diff;
    double rate = 16000, f0 = 0, confidence = 0;
    Svf aa1, aa2;
    int decim = 3, window = 320, maxTau = 246, absMaxTau = 320, minTau = 14, pos = 0, sinceRun = 0, sinceExact = 0, hop = 64;
    unsigned phase = 0;
};

// TD-PSOLA on two channels that share one set of pitch marks.
class PsolaShifter
{
public:
    void prepare (double fs, int lookAheadSamples)
    {
        sr = fs; look = lookAheadSamples;
        size = 1; while (size < int (fs * 0.25)) size <<= 1;
        mask = size - 1;
        for (auto& r : ring) r.assign (size_t (size), 0.0f);
        maxHalf = fs * 0.03;                                   // grain half-length limit (30 ms)
        reset();
    }
    void reset()
    {
        for (auto& r : ring) std::fill (r.begin(), r.end(), 0.0f);
        n = 0; for (auto& g : grains) g.active = false;
        nextSynth = 0; markPos = 0;
    }
    int latency() const noexcept { return look; }

    // ta: analysis period (input), ts: synthesis period (output), phi: formant read speed
    void process (const float* inL, const float* inR, float* outL, float* outR, int count, double ta, double ts, double phi) noexcept
    {
        for (int i = 0; i < count; ++i)
        {
            ring[0][size_t (n & mask)] = inL[i];
            ring[1][size_t (n & mask)] = inR[i];
            ++n;
            const double o = double (n - 1 - look);           // the moment being output, on the input's timeline
            if (nextSynth < o - ts) nextSynth = o;              // after a reset or a long gap
            // start every grain whose window opens now
            while (nextSynth - half (ta, ts, phi) <= o + 1.0) spawn (ta, ts, phi);
            double yl = 0, yr = 0;
            for (auto& g : grains)
            {
                if (! g.active) continue;
                const double d = o - g.outCentre;
                if (d >= g.half) { g.active = false; continue; }
                if (d <= -g.half) continue;
                const double w = g.gain * (0.5 + 0.5 * std::cos (pi * d / g.half));
                double p = g.inCentre + d * g.phi;
                p = std::min (p, double (n - 3));               // never read audio that has not arrived
                yl += w * read (0, p); yr += w * read (1, p);
            }
            outL[i] = float (yl); outR[i] = float (yr);
        }
    }
private:
    struct Grain { double inCentre = 0, outCentre = 0, half = 1, phi = 1, gain = 1; bool active = false; };
    // Hann half-length: one period, or the synthesis spacing when that is longer (pitch down),
    // shortened when raised formants would read past the look-ahead
    double half (double ta, double /*ts*/, double phi) const noexcept
    {
        // one period: a longer window would carry several periods and keep the original pitch
        double h = std::min (ta, maxHalf);
        if (phi > 1.0) h = std::min (h, std::max (0.25 * ta, (look - 0.5 * ta - 4.0) / (phi - 1.0)));
        return std::max (h, 16.0);
    }
    void spawn (double ta, double ts, double phi) noexcept
    {
        const double s = nextSynth;
        // analysis mark: the pitch mark (marks are one input period apart) nearest the synthesis time
        if (std::abs (markPos - s) > 2.0 * ta || std::abs (ts - ta) < 1e-3 * ta) markPos = s;   // no shift: exact pass-through
        while (markPos + 0.5 * ta < s) markPos += ta;
        while (markPos - 0.5 * ta > s) markPos -= ta;
        const double h = half (ta, ts, phi);
        for (auto& g : grains)
            if (! g.active)
            {
                // Hann grains of half-length h spaced ts sum to h / ts: normalise when they overlap;
                // when lowering pitch they no longer overlap, so keep the loudness (power) instead
                g = { markPos, s, h, phi, ts <= h ? ts / h : std::sqrt (ts / h), true };
                break;
            }
        nextSynth += ts;
    }
    float read (int ch, double p) const noexcept
    {
        // 4-point cubic Hermite
        const auto i = (long long) std::floor (p); const float f = float (p - double (i));
        const auto& r = ring[size_t (ch)];
        const float y0 = r[size_t ((i - 1) & mask)], y1 = r[size_t (i & mask)], y2 = r[size_t ((i + 1) & mask)], y3 = r[size_t ((i + 2) & mask)];
        const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + y1;
    }
    std::array<std::vector<float>, 2> ring;
    std::array<Grain, 24> grains {};
    long long n = 0, mask = 0;
    double sr = 48000, nextSynth = 0, markPos = 0, maxHalf = 1440;
    int size = 0, look = 480;
};

class VoiceProcessor
{
public:
    void prepare (double fs)
    {
        sr = fs;
        psola.prepare (fs, int (std::lround (0.010 * fs)));    // 10 ms look-ahead
        latency = psola.latency() + Oversampler<Lambda>::latency (2);
        tracker.prepare (fs);
        for (auto& o : os) o.prepare();
        for (auto& d : delayLine) d.prepare (size_t (Oversampler<Lambda>::latency (2)) + 8);
        reset();
    }
    void reset()
    {
        psola.reset(); tracker.reset(); for (auto& o : os) o.reset();
        for (auto& d : delayLine) d.reset();
        osWarm = { { false, false } }; driveFade = 0;
        for (auto& f : tone1) f.reset(); for (auto& f : tone2) f.reset();
        ratioLog = 0; heldLog = 0; detected = 0; period = sr / 150.0; voicedHold = 0; targetLogHz = 0; haveTarget = false;
    }
    int latencySamples() const noexcept { return latency; }
    double detectedHz() const noexcept { return detected; }
    double appliedSemitones() const noexcept { return 12 * ratioLog / std::log (2.0); }

    void process (const std::array<const float*, 2>& in, const std::array<float*, 2>& out, int n, const Values& v) noexcept
    {
        const int mode = std::clamp (int (std::round (v[voice::mode])), 0, 2);
        const bool link = v[voice::link] > 0.5;
        // the Voice selector narrows the tracker to the singer's range (fewer octave errors)
        static constexpr double lo[4] = { 65, 55, 85, 140 }, hi[4] = { 1100, 450, 700, 1100 };
        const int range = std::clamp (int (std::round (v[voice::range])), 0, 3);
        tracker.setRange (lo[range], hi[range]);

        // process in short slices so pitch decisions follow the tracker closely
        for (int start = 0; start < n; start += slice)
        {
            const int m = std::min (slice, n - start);
            for (int i = 0; i < m; ++i)
                if (tracker.push (0.5 * (in[0][start + i] + in[1][start + i])))
                {
                    if (tracker.voiced() > 0.55)
                    {
                        detected = tracker.frequency();
                        voicedHold = int (0.06 * sr);           // ride over short dropouts (60 ms)
                    }
                }
            voicedHold = std::max (0, voicedHold - m);
            const bool voiced = voicedHold > 0 && detected > 0;

            // periods in samples; unvoiced sound (breath, s, t) is carried by fixed 6 ms grains, unshifted
            if (voiced) period += (sr / detected - period) * 0.5;
            const double ta = voiced ? period : 0.006 * sr;
            double ts = ta;
            if (mode == 0)
            {
                ratioLog += (v[voice::pitch] / 12.0 * std::log (2.0) - ratioLog) * 0.15;
                if (voiced) ts = ta / std::exp (ratioLog);
            }
            else if (voiced)
            {
                // Quantize / Robot: the output period is set directly from the target note, so the
                // result is exactly on pitch (and dead flat in Robot) whatever the singer does
                double target;
                if (mode == 1)
                {
                    const double shifted = detected * std::exp2 (v[voice::pitch] / 12.0);
                    target = 440.0 * std::exp2 (std::round (12 * std::log2 (shifted / 440.0)) / 12.0);   // nearest semitone
                }
                else
                {
                    // hold the voice on one note: the Pitch knob's pitch class (0 = C), in the octave nearest the singer
                    target = 523.2511 * std::exp2 (v[voice::pitch] / 12.0);
                    while (target > detected * 1.4142) target *= 0.5;
                    while (target < detected / 1.4142) target *= 2.0;
                }
                const double logT = std::log (target);
                targetLogHz = haveTarget ? targetLogHz + (logT - targetLogHz) * 0.5 : logT;
                haveTarget = true;
                ts = sr / std::exp (targetLogHz);
                ratioLog = std::log (ta / ts);
            }
            const double ratio = ta / ts;
            const double phi = link ? (voiced ? ratio : 1.0) * std::exp2 (v[voice::formant] / 12.0)
                                    : std::exp2 (v[voice::formant] / 12.0);
            psola.process (in[0] + start, in[1] + start, work[0].data(), work[1].data(), m, ta, ts, phi);

            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < m; ++i) out[size_t (c)][start + i] = work[size_t (c)][size_t (i)];
        }

        // Drive: tube stage, oversampled 2x. At 0 dB it is switched out (a plain delay of the same
        // length keeps the timing) and cross-faded in/out over 20 ms when the knob moves.
        drive = gain (v[voice::drive]) * 0.5;
        comp = 1.0 / std::max (1e-3, curveGain ([] (double u) { return curves::valveStage1 (u); }, drive));
        const double driveTarget = v[voice::drive] > 0.05 ? 1.0 : 0.0;
        const double fadeStep = 1.0 / (0.02 * sr);
        const bool toneOn = v[voice::tone] < 19500;
        const int osLat = Oversampler<Lambda>::latency (2);
        double fadeEnd = driveFade;
        for (int c = 0; c < 2; ++c)
        {
            tone1[size_t (c)].set (Svf::LowPass, sr, std::min (v[voice::tone], 0.45 * sr), 0.5412);
            tone2[size_t (c)].set (Svf::LowPass, sr, std::min (v[voice::tone], 0.45 * sr), 1.3066);
            fn[size_t (c)].owner = this;
            double fade = driveFade;
            for (int i = 0; i < n; ++i)
            {
                const double x = out[size_t (c)][i];
                delayLine[size_t (c)].push (x);
                const double clean = delayLine[size_t (c)].age (osLat);
                fade = driveTarget > fade ? std::min (driveTarget, fade + fadeStep) : std::max (driveTarget, fade - fadeStep);
                double y = clean;
                if (fade > 0.0)
                {
                    const double hot = os[size_t (c)].tick (x, 2, fn[size_t (c)]);
                    y = clean + fade * (hot - clean);
                }
                else if (osWarm[size_t (c)]) { os[size_t (c)].reset(); osWarm[size_t (c)] = false; }
                if (fade > 0.0) osWarm[size_t (c)] = true;
                if (toneOn) y = tone2[size_t (c)].tick (tone1[size_t (c)].tick (y));
                out[size_t (c)][i] = float (finite (y));
            }
            fadeEnd = fade;
        }
        driveFade = fadeEnd;
    }
private:
    struct Lambda { VoiceProcessor* owner = nullptr; double operator() (double u) const noexcept { return curves::valveStage1 (u * owner->drive) * owner->comp; } };
    static constexpr int slice = 32;
    PsolaShifter psola;
    PitchTracker tracker;
    std::array<Oversampler<Lambda>, 2> os; std::array<Lambda, 2> fn;
    std::array<Svf, 2> tone1, tone2;
    std::array<Ring, 2> delayLine;
    std::array<bool, 2> osWarm { { false, false } };
    double driveFade = 0;
    std::array<std::array<float, slice>, 2> work {};
    double sr = 48000, ratioLog = 0, heldLog = 0, detected = 0, drive = 0.5, comp = 1, period = 320;
    int latency = 0, voicedHold = 0;
    double targetLogHz = 0;
    bool haveTarget = false;
};
}
