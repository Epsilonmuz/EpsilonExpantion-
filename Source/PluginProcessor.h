#pragma once
// Shared processor for all nine Epsilon effects. Factory.cpp picks the effect with EPSILON_EFFECT_INDEX.
#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_audio_utils/juce_audio_utils.h>
#endif
#include "DSP/Definitions.h"
#include "DSP/Presets.h"

namespace eps { class EffectEngine; }

class ExpansionProcessor final : public juce::AudioProcessor
{
public:
    explicit ExpansionProcessor (eps::Effect);
    ~ExpansionProcessor() override;
    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    using juce::AudioProcessor::processBlock;
    using juce::AudioProcessor::processBlockBypassed;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Epsilon " + juce::String (info.name).toLowerCase().replaceSection (0, 1, juce::String (info.name).substring (0, 1)); }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override;
    int getNumPrograms() override { return eps::presetCount; }
    int getCurrentProgram() override { return program.load(); }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int i) override { return eps::presetName (effect, juce::jlimit (0, eps::presetCount - 1, i)); }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    eps::Values values() const noexcept;
    void applyValues (const eps::Values&);
    juce::RangedAudioParameter* parameter (int slot) const;
    double latencyMilliseconds() const noexcept { return 1000.0 * getLatencySamples() / processingRate.load (std::memory_order_relaxed); }
    double sampleRateNow() const noexcept { return processingRate.load (std::memory_order_relaxed); }

    const eps::Effect effect;
    const eps::Descriptor info;
    juce::AudioProcessorValueTreeState state;

    // meters for the editor (peaks are reset by the reader)
    std::atomic<float> inputPeak { 0 }, outputPeak { 0 }, reduction { 0 }, driveLevel { -100 }, bpmDisplay { 120 };
    std::atomic<float> voiceHz { 0 }, voiceShift { 0 };
    std::atomic<bool> hostTempo { false };
    std::array<std::atomic<float>, 512> scope {};
    std::atomic<unsigned> scopeWrite { 0 };
    // full-rate mono taps of the input and output for the spectrum analyser
    static constexpr unsigned analyserSize = 4096;
    std::array<std::atomic<float>, analyserSize> analyserIn {}, analyserOut {};
    std::atomic<unsigned> analyserWrite { 0 };
    double analyserRate() const { return processingRate.load(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout (eps::Effect);
    void process (juce::AudioBuffer<float>&, bool hostBypass);
    std::unique_ptr<eps::EffectEngine> engine;
    std::array<std::atomic<float>*, eps::slots> raw {};
    std::atomic<int> program { 0 };
    std::atomic<double> processingRate { 48000 };
    unsigned scopeCounter = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExpansionProcessor)
};
