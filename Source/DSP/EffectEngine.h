#pragma once
// One engine for all nine effects: parameter smoothing, dry/wet alignment, mix, output, bypass, meters.
#include "ColourProcessors.h"
#include "DelayProcessors.h"
#include "Reverbs.h"
#include "VoiceProcessor.h"
#include <memory>

namespace eps {
class EffectEngine
{
public:
    explicit EffectEngine (Effect which) : effect (which), desc (descriptor (which)), target (defaults (which)), v (target) {}

    void prepare (double sampleRate, const Values& initial)
    {
        sr = std::isfinite (sampleRate) ? std::clamp (sampleRate, 8000.0, 192000.0) : 48000.0;
        smoothing = 1 - std::exp (-1 / (0.015 * sr));
        setParameters (initial, true);
        switch (effect)
        {
            case Effect::Heat: case Effect::Valve: case Effect::Crush: case Effect::Tone:
                colour = std::make_unique<ColourProcessor>(); colour->prepare (effect, sr); latency = ColourProcessor::latency(); break;
            case Effect::Prism: prism = std::make_unique<PrismProcessor>(); prism->prepare (sr); latency = 0; break;
            case Effect::Time:  time = std::make_unique<TimeProcessor>(); time->prepare (sr); latency = 0; break;
            case Effect::Plate: plate = std::make_unique<PlateProcessor>(); plate->prepare (sr); latency = 0; break;
            case Effect::Space: space = std::make_unique<SpaceProcessor>(); space->prepare (sr); latency = 0; break;
            case Effect::Voice: voice = std::make_unique<VoiceProcessor>(); voice->prepare (sr); latency = voice->latencySamples(); break;
        }
        for (auto& r : dryDelay) r.prepare (size_t (latency) + 16);
        ready = true;
        reset();
    }
    void setParameters (const Values& incoming, bool immediate = false) noexcept
    {
        for (size_t i = 0; i < target.size(); ++i)
        {
            const auto& s = desc.spec[i];
            const double next = s.id ? checked (s, incoming[i]) : 0.0;
            target[i] = next;
            if (s.choices) v[i] = next;                                    // switches and choices change instantly
            smoothTarget[i] = s.log ? std::log (std::max (1e-6, next)) : next;
            if (immediate) { smoothed[i] = smoothTarget[i]; v[i] = next; }
        }
        smoothingActive = ! immediate;
    }
    void reset()
    {
        if (colour) colour->reset(); if (prism) prism->reset(); if (time) time->reset();
        if (plate) plate->reset(); if (space) space->reset(); if (voice) voice->reset();
        for (auto& r : dryDelay) r.reset();
        inPeak = outPeak = 0; bypassMix = target[bypassSlot];
    }
    int latencySamples() const noexcept { return latency; }

    void process (float* left, float* right, int n, Transport t = {}, bool hostBypass = false) noexcept
    {
        if (! ready || ! left || n <= 0) return;
        inPeak = outPeak = 0;
        forcedDry = hostBypass;
        const double bpm = t.hostTempo && std::isfinite (t.bpm) && t.bpm >= 20 && t.bpm <= 400 ? t.bpm : (target[tempoSlot] > 0 ? target[tempoSlot] : 120);
        bpmUsed = bpm;
        for (int offset = 0; offset < n;)
        {
            const int size = std::min (64, n - offset);
            if (effect == Effect::Voice)
            {
                for (int i = 0; i < size; ++i)
                {
                    inputWork[0][size_t (i)] = float (finite (left[offset + i]));
                    inputWork[1][size_t (i)] = right ? float (finite (right[offset + i])) : inputWork[0][size_t (i)];
                }
                advance (size);
                std::array<const float*, 2> in { { inputWork[0].data(), inputWork[1].data() } };
                std::array<float*, 2> out { { outputWork[0].data(), outputWork[1].data() } };
                voice->process (in, out, size, v);
                for (int i = 0; i < size; ++i)
                    finish (left, right, offset + i, { inputWork[0][size_t (i)], inputWork[1][size_t (i)] }, { outputWork[0][size_t (i)], outputWork[1][size_t (i)] });
            }
            else
                for (int i = 0; i < size; ++i)
                {
                    advance (1);
                    const double l = finite (left[offset + i]), r = right ? finite (right[offset + i]) : l;
                    Stereo x { l, r }, wet;
                    if (colour) wet = colour->tick (x, v);
                    else if (prism) wet = prism->tick (x, v, bpm);
                    else if (time) wet = time->tick (x, v, bpm);
                    else if (plate) wet = plate->tick (x, v);
                    else wet = space->tick (x, v, bpm);
                    finish (left, right, offset + i, x, wet);
                }
            offset += size;
        }
    }
    // meters and readouts for the editor
    double inputPeak() const noexcept { return inPeak; }
    double outputPeak() const noexcept { return outPeak; }
    double reduction() const noexcept { return colour ? colour->reduction() : 0.0; }
    double driveMeter() const noexcept { return colour ? colour->meter() : -100.0; }
    double voicePitchHz() const noexcept { return voice ? voice->detectedHz() : 0.0; }
    double voiceShift() const noexcept { return voice ? voice->appliedSemitones() : 0.0; }
    double tempo() const noexcept { return bpmUsed; }
    const Descriptor& info() const noexcept { return desc; }

private:
    void advance (int n) noexcept
    {
        if (! smoothingActive) return;
        const double a = 1 - std::pow (1 - smoothing, n);
        bool moving = false;
        for (size_t i = 0; i < v.size(); ++i)
        {
            if (desc.spec[i].choices || ! desc.spec[i].id) continue;
            smoothed[i] += a * (smoothTarget[i] - smoothed[i]);
            moving |= std::abs (smoothed[i] - smoothTarget[i]) > 1e-9;
            v[i] = desc.spec[i].log ? std::exp (smoothed[i]) : smoothed[i];
        }
        if (! moving) { for (size_t i = 0; i < v.size(); ++i) if (! desc.spec[i].choices) v[i] = target[i]; smoothed = smoothTarget; smoothingActive = false; }
    }
    void finish (float* left, float* right, int i, Stereo input, Stereo wet) noexcept
    {
        inPeak = std::max (inPeak, std::max (std::abs (input.l), std::abs (input.r)));
        dryDelay[0].push (input.l); dryDelay[1].push (input.r);
        const Stereo dry { latency ? dryDelay[0].age (latency) : input.l, latency ? dryDelay[1].age (latency) : input.r };
        Stereo mixed = blend (dry, wet, v[mixSlot]);
        const double trim = gain (v[outSlot]);
        mixed.l *= trim; mixed.r *= trim;
        // the Bypass control cross-fades to the latency-aligned dry signal
        bypassMix += ((target[bypassSlot] > 0.5 ? 1.0 : 0.0) - bypassMix) * 0.002;
        mixed = forcedDry ? dry : blend (mixed, dry, bypassMix);
        left[i] = float (finite (right ? mixed.l : 0.5 * (mixed.l + mixed.r)));
        if (right) right[i] = float (finite (mixed.r));
        outPeak = std::max (outPeak, std::max (std::abs (mixed.l), std::abs (mixed.r)));
    }
    Effect effect;
    Descriptor desc;
    Values target {}, v {}, smoothed {}, smoothTarget {};
    double sr = 48000, smoothing = 0.001, inPeak = 0, outPeak = 0, bypassMix = 0, bpmUsed = 120;
    int latency = 0;
    bool ready = false, forcedDry = false, smoothingActive = false;
    std::unique_ptr<ColourProcessor> colour;
    std::unique_ptr<PrismProcessor> prism;
    std::unique_ptr<TimeProcessor> time;
    std::unique_ptr<PlateProcessor> plate;
    std::unique_ptr<SpaceProcessor> space;
    std::unique_ptr<VoiceProcessor> voice;
    std::array<Ring, 2> dryDelay;
    std::array<std::array<float, 64>, 2> inputWork {}, outputWork {};
};
}
