#pragma once
// VOICE: pitch and formant processor with Transpose, Quantize and Robot modes.
// Pitch/formant shifting by Signalsmith Stretch (MIT). Quantize and Robot use a YIN pitch tracker.
#include "ColourProcessors.h"
#include "../../ThirdParty/signalsmith-stretch.h"

namespace eps {

// Streaming YIN (de Cheveigne & Kawahara 2002) on a mono signal decimated to about 16 kHz.
class PitchTracker
{
public:
    void prepare (double fs)
    {
        decim = std::max (1, int (fs / 16000.0));              // analyse at about 16 kHz whatever the host rate
        rate = fs / decim;
        aa1.set (Svf::LowPass, fs, 0.4 * rate, 0.5412); aa2.set (Svf::LowPass, fs, 0.4 * rate, 1.3066);
        window = int (std::ceil (0.022 * rate));
        maxTau = int (rate / 65.0);
        minTau = std::max (2, int (rate / 1100.0));
        hist.assign (size_t (window + maxTau + 4), 0.0f);
        diffBuf.assign (size_t (maxTau + 2), 0.0);
        reset();
    }
    void reset() { std::fill (hist.begin(), hist.end(), 0.0f); pos = 0; phase = 0; f0 = 0; confidence = 0; aa1.reset(); aa2.reset(); }
    // feed one sample at the full rate; returns true when a new estimate is available
    bool push (double x) noexcept
    {
        const double y = aa2.tick (aa1.tick (x));                     // anti-alias for the decimation
        if (int (phase++ % unsigned (decim)) != 0) return false;
        hist[size_t (pos)] = float (y); pos = (pos + 1) % int (hist.size());
        if (++sinceRun < hop) return false;
        sinceRun = 0; estimate(); return true;
    }
    double frequency() const noexcept { return f0; }
    double voiced() const noexcept { return confidence; }
private:
    float at (int ago) const noexcept { const int n = int (hist.size()); return hist[size_t ((pos - 1 - ago + 2 * n) % n)]; }
    void estimate() noexcept
    {
        double energy = 0;
        for (int i = 0; i < window; ++i) energy += double (at (i)) * at (i);
        if (energy < window * 1e-7) { confidence = 0; return; }
        double running = 0;
        int best = -1; double bestVal = 1;
        for (int t = 1; t <= maxTau; ++t)
        {
            double d = 0;
            for (int i = 0; i < window; ++i) { const double q = double (at (i)) - at (i + t); d += q * q; }
            running += d;
            diffBuf[size_t (t)] = d * t / std::max (1e-12, running);
        }
        for (int t = minTau; t < maxTau; ++t)
            if (diffBuf[size_t (t)] < 0.12) { while (t + 1 < maxTau && diffBuf[size_t (t + 1)] < diffBuf[size_t (t)]) ++t; best = t; bestVal = diffBuf[size_t (t)]; break; }
        if (best < 0) { confidence = 0; return; }
        const double a = diffBuf[size_t (best - 1)], b = diffBuf[size_t (best)], c = diffBuf[size_t (best + 1)];
        const double den = a - 2 * b + c;
        const double off = std::abs (den) > 1e-12 ? 0.5 * (a - c) / den : 0.0;
        f0 = rate / (best + std::clamp (off, -1.0, 1.0));
        confidence = 1 - bestVal;
    }
    std::vector<float> hist; std::vector<double> diffBuf;
    double rate = 16000, f0 = 0, confidence = 0;
    Svf aa1, aa2;
    int decim = 3, window = 352, maxTau = 246, minTau = 14, pos = 0, sinceRun = 0, hop = 96;
    unsigned phase = 0;
};

class VoiceProcessor
{
public:
    void prepare (double fs)
    {
        sr = fs;
        int block = 1024; while (block < int (sr * 0.04)) block *= 2;
        stretch.configure (2, block, block / 16, true);
        latency = stretch.inputLatency() + stretch.outputLatency() + Oversampler<Lambda>::latency (2);
        tracker.prepare (fs);
        for (auto& o : os) o.prepare();
        reset();
    }
    void reset()
    {
        stretch.reset(); tracker.reset(); for (auto& o : os) o.reset();
        for (auto& f : tone1) f.reset(); for (auto& f : tone2) f.reset();
        ratioLog = 0; targetLog = 0; heldLog = 0; detected = 0;
    }
    int latencySamples() const noexcept { return latency; }
    double detectedHz() const noexcept { return detected; }
    double appliedSemitones() const noexcept { return 12 * ratioLog / std::log (2.0); }

    void process (const std::array<const float*, 2>& in, const std::array<float*, 2>& out, int n, const Values& v) noexcept
    {
        const int mode = std::clamp (int (std::round (v[voice::mode])), 0, 2);
        const bool link = v[voice::link] > 0.5;
        // pitch tracking for Quantize / Robot
        for (int i = 0; i < n; ++i)
            if (tracker.push (0.5 * (in[0][i] + in[1][i])) && tracker.voiced() > 0.5)
            {
                const double f = tracker.frequency(); detected = f;
                const double shifted = f * std::exp2 (v[voice::pitch] / 12.0);
                if (mode == 1)
                {
                    const double note = std::round (12 * std::log2 (shifted / 440.0));        // nearest semitone
                    heldLog = std::log (440.0 * std::exp2 (note / 12.0) / f);
                }
                else if (mode == 2)
                {
                    // hold the vocal on one note: the Pitch knob's pitch class (0 = C), in the octave nearest the singer
                    double target = 523.2511 * std::exp2 (v[voice::pitch] / 12.0);
                    while (target > f * 1.4142) target *= 0.5;
                    while (target < f / 1.4142) target *= 2.0;
                    heldLog = std::log (target / f);
                }
            }
        targetLog = mode == 0 ? v[voice::pitch] / 12.0 * std::log (2.0) : heldLog;
        const double speed = mode == 0 ? 0.25 : 0.6;                                         // Quantize/Robot snap fast
        ratioLog += (targetLog - ratioLog) * speed;
        stretch.setTransposeFactor (float (std::exp (ratioLog)));
        stretch.setFormantFactor (float (std::exp2 (v[voice::formant] / 12.0)), ! link);
        static constexpr double base[4] = { 0, 110, 200, 320 };
        stretch.setFormantBase (float (base[size_t (std::clamp (int (std::round (v[voice::range])), 0, 3))] / sr));
        std::array<float*, 2> shifted { { work[0].data(), work[1].data() } };
        stretch.process (in, n, shifted, n);

        drive = gain (v[voice::drive]) * 0.5;
        comp = 1.0 / std::max (1e-3, curveGain ([] (double u) { return curves::valveStage1 (u); }, drive));
        const bool toneOn = v[voice::tone] < 19500;
        for (int c = 0; c < 2; ++c)
        {
            tone1[size_t (c)].set (Svf::LowPass, sr, std::min (v[voice::tone], 0.45 * sr), 0.5412);
            tone2[size_t (c)].set (Svf::LowPass, sr, std::min (v[voice::tone], 0.45 * sr), 1.3066);
            fn[size_t (c)].owner = this;
            for (int i = 0; i < n; ++i)
            {
                double y = work[size_t (c)][size_t (i)];
                y = os[size_t (c)].tick (y, 2, fn[size_t (c)]);
                if (toneOn) y = tone2[size_t (c)].tick (tone1[size_t (c)].tick (y));
                out[size_t (c)][i] = float (finite (y));
            }
        }
    }
private:
    struct Lambda { VoiceProcessor* owner = nullptr; double operator() (double u) const noexcept { return curves::valveStage1 (u * owner->drive) * owner->comp; } };
    signalsmith::stretch::SignalsmithStretch<float> stretch { 1176 };
    PitchTracker tracker;
    std::array<Oversampler<Lambda>, 2> os; std::array<Lambda, 2> fn;
    std::array<Svf, 2> tone1, tone2;
    std::array<std::array<float, 128>, 2> work {};
    double sr = 48000, ratioLog = 0, targetLog = 0, heldLog = 0, detected = 0, drive = 0.5, comp = 1;
    int latency = 0;
};
}
