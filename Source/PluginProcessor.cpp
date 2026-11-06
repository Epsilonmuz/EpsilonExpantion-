#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "DSP/EffectEngine.h"

namespace
{
juce::String unitText (const eps::Spec& s, double v)
{
    using U = eps::Unit;
    switch (s.unit)
    {
        case U::Db:        return (v > 0.05 ? "+" : "") + juce::String (v, 1) + " dB";
        case U::Hz:        return v >= 1000 ? juce::String (v / 1000.0, v >= 10000 ? 1 : 2) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz";
        case U::Ms:        return v >= 1000 ? juce::String (v / 1000.0, 2) + " s" : juce::String (v, v < 100 ? 1 : 0) + " ms";
        case U::Seconds:   return v >= 10 ? juce::String (v, 1) + " s" : juce::String (v, 2) + " s";
        case U::Semitones: return (v > 0.005 ? "+" : "") + juce::String (v, 1) + " st";
        case U::Percent:   return juce::String (juce::roundToInt (v * 100)) + " %";
        case U::Bpm:       return juce::String (v, 1) + " BPM";
        case U::Cents:     return juce::String (juce::roundToInt (v)) + " ct";
        case U::Bipolar:   return std::abs (v) < 0.005 ? juce::String ("0") : (v > 0 ? "+" : "") + juce::String (v, 2);
        default:           return juce::String (v, 1);
    }
}
}

juce::AudioProcessorValueTreeState::ParameterLayout ExpansionProcessor::layout (eps::Effect e)
{
    juce::AudioProcessorValueTreeState::ParameterLayout result;
    const auto d = eps::descriptor (e);
    for (const auto& s : d.spec)
    {
        if (! s.id) continue;
        const juce::ParameterID id { s.id, 2 };
        if (s.choices)
        {
            // Discrete parameters are stepped floats: the stored value is always snapped to a whole step,
            // so a host (or pluginval) can never leave a switch half-way and a saved session restores exactly.
            juce::StringArray names; names.addTokens (s.choices, "|", "");
            const bool isToggle = names.size() == 2 && names[0] == "Off";
            auto attrs = juce::AudioProcessorValueTreeStateParameterAttributes()
                .withStringFromValueFunction ([names] (float v, int) { return names[juce::jlimit (0, names.size() - 1, juce::roundToInt (v))]; })
                .withValueFromStringFunction ([names] (const juce::String& t) { const int i = names.indexOf (t, true); return float (i >= 0 ? i : t.getIntValue()); })
                .withDiscrete (true).withBoolean (isToggle).withAutomatable (true);
            result.add (std::make_unique<juce::AudioProcessorValueTreeState::Parameter> (id, s.name, juce::NormalisableRange<float> (float (s.lo), float (s.hi), 1.0f), float (s.initial), attrs));
        }
        else
        {
            juce::NormalisableRange<float> range (float (s.lo), float (s.hi));
            if (s.log) range.setSkewForCentre (float (std::sqrt (s.lo * s.hi)));
            const eps::Spec spec = s;
            auto attrs = juce::AudioParameterFloatAttributes()
                .withStringFromValueFunction ([spec] (float v, int) { return unitText (spec, double (v)); })
                .withValueFromStringFunction ([] (const juce::String& t) { auto x = t.getFloatValue(); return t.containsIgnoreCase ("khz") ? x * 1000.0f : t.endsWithIgnoreCase (" s") && ! t.containsIgnoreCase ("ms") ? x : x; });
            result.add (std::make_unique<juce::AudioParameterFloat> (id, s.name, range, float (s.initial), attrs));
        }
    }
    return result;
}

ExpansionProcessor::ExpansionProcessor (eps::Effect e)
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      effect (e), info (eps::descriptor (e)), state (*this, nullptr, "EpsilonExpansion", layout (e)),
      engine (std::make_unique<eps::EffectEngine> (e))
{
    for (size_t i = 0; i < raw.size(); ++i) if (info.spec[i].id) raw[i] = state.getRawParameterValue (info.spec[i].id);
}
ExpansionProcessor::~ExpansionProcessor() = default;

void ExpansionProcessor::reset() { engine->reset(); }
juce::RangedAudioParameter* ExpansionProcessor::parameter (int slot) const
{
    return info.spec[size_t (slot)].id ? state.getParameter (info.spec[size_t (slot)].id) : nullptr;
}
eps::Values ExpansionProcessor::values() const noexcept
{
    eps::Values v {};
    for (size_t i = 0; i < v.size(); ++i) v[i] = raw[i] ? double (raw[i]->load (std::memory_order_relaxed)) : 0.0;
    return v;
}
void ExpansionProcessor::applyValues (const eps::Values& v)
{
    for (size_t i = 0; i < v.size(); ++i)
        if (info.spec[i].id && i != size_t (eps::bypassSlot))
            if (auto* p = state.getParameter (info.spec[i].id))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost (p->convertTo0to1 (float (eps::checked (info.spec[i], v[i]))));
                p->endChangeGesture();
            }
}
void ExpansionProcessor::prepareToPlay (double fs, int)
{
    fs = std::isfinite (fs) ? juce::jlimit (8000.0, 192000.0, fs) : 48000.0;
    processingRate.store (fs, std::memory_order_relaxed);
    engine->prepare (fs, values());
    setLatencySamples (engine->latencySamples());
    scopeCounter = 0; scopeWrite = 0;
    for (auto& x : scope) x = 0;
}
bool ExpansionProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return l.getMainInputChannelSet() == out && (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo());
}
void ExpansionProcessor::processBlock (juce::AudioBuffer<float>& b, juce::MidiBuffer&) { process (b, false); }
void ExpansionProcessor::processBlockBypassed (juce::AudioBuffer<float>& b, juce::MidiBuffer&) { process (b, true); }

void ExpansionProcessor::process (juce::AudioBuffer<float>& b, bool hostBypass)
{
    juce::ScopedNoDenormals noDenormals;
    if (b.getNumChannels() == 0 || b.getNumSamples() == 0) return;
    for (int c = 2; c < b.getNumChannels(); ++c) b.clear (c, 0, b.getNumSamples());
    const auto v = values();
    engine->setParameters (v);
    eps::Transport t;
    t.bpm = v[eps::tempoSlot] > 0 ? v[eps::tempoSlot] : 120;
    if (auto* head = getPlayHead())
        if (auto pos = head->getPosition())
        {
            if (auto bpm = pos->getBpm()) if (std::isfinite (*bpm) && *bpm >= 20 && *bpm <= 400) { t.bpm = *bpm; t.hostTempo = true; }
            if (auto ppq = pos->getPpqPosition()) { t.ppq = *ppq; t.hasPpq = std::isfinite (*ppq); }
            t.playing = pos->getIsPlaying();
        }
    {
        unsigned w = analyserWrite.load (std::memory_order_relaxed);
        const int nc = b.getNumChannels();
        for (int i = 0; i < b.getNumSamples(); ++i, ++w)
            analyserIn[w & (analyserSize - 1)].store (nc > 1 ? 0.5f * (b.getSample (0, i) + b.getSample (1, i)) : b.getSample (0, i), std::memory_order_relaxed);
    }
    hostTempo.store (t.hostTempo, std::memory_order_relaxed);
    engine->process (b.getWritePointer (0), b.getNumChannels() > 1 ? b.getWritePointer (1) : nullptr, b.getNumSamples(), t, hostBypass);
    bpmDisplay.store (float (engine->tempo()), std::memory_order_relaxed);
    auto peak = [] (std::atomic<float>& meter, float value)
    {
        float previous = meter.load (std::memory_order_relaxed);
        while (previous < value && ! meter.compare_exchange_weak (previous, value, std::memory_order_relaxed)) {}
    };
    peak (inputPeak, float (engine->inputPeak()));
    peak (outputPeak, float (engine->outputPeak()));
    reduction.store (float (engine->reduction()), std::memory_order_relaxed);
    driveLevel.store (float (engine->driveMeter()), std::memory_order_relaxed);
    voiceHz.store (float (engine->voicePitchHz()), std::memory_order_relaxed);
    voiceShift.store (float (engine->voiceShift()), std::memory_order_relaxed);
    {
        unsigned w = analyserWrite.load (std::memory_order_relaxed);
        const int nc = b.getNumChannels();
        for (int i = 0; i < b.getNumSamples(); ++i, ++w)
            analyserOut[w & (analyserSize - 1)].store (nc > 1 ? 0.5f * (b.getSample (0, i) + b.getSample (1, i)) : b.getSample (0, i), std::memory_order_relaxed);
        analyserWrite.store (w, std::memory_order_relaxed);
    }
    unsigned position = scopeWrite.load (std::memory_order_relaxed);
    for (int i = 0; i < b.getNumSamples(); ++i)
        if ((scopeCounter++ & 7u) == 0) { scope[position & 511u].store (b.getSample (0, i), std::memory_order_relaxed); ++position; }
    scopeWrite.store (position, std::memory_order_relaxed);
}

void ExpansionProcessor::setCurrentProgram (int i)
{
    i = juce::jlimit (0, eps::presetCount - 1, i);
    applyValues (eps::preset (effect, i));
    program.store (i);
}
void ExpansionProcessor::getStateInformation (juce::MemoryBlock& out)
{
    auto tree = state.copyState();
    tree.setProperty ("effect", int (effect), nullptr);
    tree.setProperty ("preset", program.load(), nullptr);
    tree.setProperty ("version", "0.2.0", nullptr);
    if (auto xml = tree.createXml()) copyXmlToBinary (*xml, out);
}
void ExpansionProcessor::setStateInformation (const void* d, int n)
{
    if (auto xml = getXmlFromBinary (d, n))
        if (xml->hasTagName ("EpsilonExpansion"))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            if (tree.isValid() && int (tree.getProperty ("effect", -1)) == int (effect))
            {
                state.replaceState (tree);
                program.store (juce::jlimit (0, eps::presetCount - 1, int (tree.getProperty ("preset", 0))));
            }
        }
}
double ExpansionProcessor::getTailLengthSeconds() const
{
    const auto p = values();
    double bpm = hostTempo.load() ? double (bpmDisplay.load()) : (p[eps::tempoSlot] > 0 ? p[eps::tempoSlot] : 120);
    if (! std::isfinite (bpm) || bpm < 20 || bpm > 400) bpm = 120;
    auto repeats = [] (double fb) { return fb > 0.001 && fb < 0.999 ? std::log (0.001) / std::log (fb) : 40.0; };
    using namespace eps;
    switch (effect)
    {
        case Effect::Plate: return p[plate::freeze] > 0.5 ? 3600.0 : p[plate::decay] * 1.2 + p[plate::predelay] * 0.001 + 0.5;
        case Effect::Space: return p[space::freeze] > 0.5 ? 3600.0 : p[space::time] * 1.3 + 1.0;
        case Effect::Time:
        {
            if (p[time::freeze] > 0.5) return 3600.0;
            const double d = std::max (p[time::timeA], p[time::timeB]) * 0.001 * double (1 << int (p[time::multiply]));
            return std::min (120.0, d * (1 + repeats (std::max (p[time::feedbackA], p[time::feedbackB])))) + 0.3;
        }
        case Effect::Prism: return std::min (120.0, (p[prism::splice] + p[prism::delay]) * 0.002 * (1 + repeats (p[prism::recycle]))) + 0.3;
        default: return latencyMilliseconds() * 0.001 + 0.05;
    }
}
juce::AudioProcessorEditor* ExpansionProcessor::createEditor() { return new ExpansionEditor (*this); }
