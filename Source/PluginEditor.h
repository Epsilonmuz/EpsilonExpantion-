#pragma once
#include "PluginProcessor.h"
#include "UI/Kit.h"
#include "UI/Displays.h"

class ExpansionEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ExpansionEditor (ExpansionProcessor&);
    ~ExpansionEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void refreshForSnapshot (int frames);       // used by the headless snapshot tool
    void setTweakVisible (bool);
    // for the UI performance tool: the parts that redraw every frame
    std::vector<juce::Component*> animatedParts() { return { display.get(), spectrum.get(), &inMeter, &outMeter }; }

private:
    struct Layout
    {
        int hero = -1;
        std::vector<int> secondary, strip, toggles, bottom, tweak;
    };
    static Layout layoutFor (eps::Effect);
    void timerCallback() override;
    void buildPresetMenu();
    void loadPreset (int index);
    void switchBank (int bank);
    juce::Rectangle<int> sc (float x, float y, float w, float h) const;
    juce::Component* controlFor (int slot);

    ExpansionProcessor& proc;
    eps::ui::Theme theme;
    eps::ui::LookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 650 };
    Layout layout;
    float s = 1.0f;

    std::unique_ptr<eps::ui::Display> display;
    std::unique_ptr<eps::ui::Spectrum> spectrum;
    juce::Rectangle<float> spectrumPanel;
    std::vector<std::unique_ptr<eps::ui::Knob>> knobs;
    std::vector<std::unique_ptr<eps::ui::LedSwitch>> switches;
    std::vector<std::unique_ptr<eps::ui::Segmented>> segments;
    std::map<int, juce::Component*> bySlot;
    eps::ui::LedMeter inMeter { "IN", theme }, outMeter { "OUT", theme };

    eps::ui::PillButton prev { "<", theme }, next { ">", theme }, presetName { "", theme }, bankA { "A", theme }, bankB { "B", theme },
                        inspire { "Inspire", theme }, tweak { "Tweak", theme }, bypass { "", theme };
    std::unique_ptr<juce::ParameterAttachment> bypassAttachment;
    std::array<eps::Values, 2> banks {};
    std::array<bool, 2> bankUsed { { false, false } };
    int bank = 0;
    bool tweakVisible = false;
    juce::Image background;                     // cached faceplate, at screen pixel density
    float backgroundScale = 0.0f;
    void renderBackground (float pixelScale);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExpansionEditor)
};
