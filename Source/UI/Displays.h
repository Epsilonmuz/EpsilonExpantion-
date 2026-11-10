#pragma once
// One signature display per effect, drawn on the glass screen and refreshed at 30 Hz.
#include "Kit.h"
#include <juce_dsp/juce_dsp.h>
#include "../PluginProcessor.h"

namespace eps::ui {
class Display : public juce::Component
{
public:
    Display (ExpansionProcessor& p, const Theme& t) : proc (p), theme (t) {}
    virtual void tick() { repaint(); }               // called by the editor's timer
    float scale = 1.0f;
protected:
    juce::Rectangle<float> screen() const { return getLocalBounds().toFloat().reduced (2.0f * scale); }
    void grid (juce::Graphics&, juce::Rectangle<float>, int cols, int rows) const;
    void caption (juce::Graphics&, juce::Rectangle<float>, const juce::String& left, const juce::String& right) const;
    void scopeTrace (juce::Graphics&, juce::Rectangle<float>, float alpha) const;
    ExpansionProcessor& proc;
    Theme theme;
};
std::unique_ptr<Display> makeDisplay (ExpansionProcessor&, const Theme&);

// Real-time input / output spectrum shown along the bottom of every plugin.
class Spectrum : public juce::Component
{
public:
    Spectrum (ExpansionProcessor& p, const Theme& t);
    void tick();
    void paint (juce::Graphics&) override;
    void resized() override;
    float scale = 1.0f;
private:
    void renderStatic (float pixelScale);
    juce::Image staticLayer;
    float staticScale = 0.0f;
    void analyse (const std::array<std::atomic<float>, ExpansionProcessor::analyserSize>&, std::vector<float>& smoothed, unsigned w);
    ExpansionProcessor& proc;
    Theme theme;
    static constexpr int order = 12, size = 1 << order, points = 160;
    juce::dsp::FFT fft { order };
    std::vector<float> window, work, inDb, outDb;
    unsigned lastWrite = ~0u;
    bool settled = false, drawnOnce = false;
};
}
