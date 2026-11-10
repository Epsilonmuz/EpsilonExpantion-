#pragma once
// Epsilon UI kit: faceplate drawing, knobs, LED switches, segmented selectors, meters.
#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"
#include "../../Shared/epsilon_ui/EpsilonSignature.h"

namespace eps::ui {

// ------------------------------------------------------------------ drawing helpers
namespace draw {
void faceplate (juce::Graphics&, juce::Rectangle<float>, const Theme&, float scale);
void recess (juce::Graphics&, juce::Rectangle<float>, const Theme&, float radius, float scale);   // engraved panel
void glass (juce::Graphics&, juce::Rectangle<float>, const Theme&, float radius, float scale);    // display screen
void screw (juce::Graphics&, juce::Point<float>, float r, float angle);
void engraved (juce::Graphics&, const juce::String&, juce::Rectangle<float>, juce::Justification, const juce::Font&, juce::Colour);
void glowStroke (juce::Graphics&, const juce::Path&, juce::Colour, float width, float glow);
}

// ------------------------------------------------------------------ rendering
// macOS's CoreGraphics is slow at the many gradients, glows and long paths an animated plug-in
// panel draws (measured on an Intel Mac: about half a CPU core at 30 fps for one display).
// Attached to the editor, this cache makes JUCE's own software renderer draw every repainted
// area into a native bitmap, which the system then shows as a single image.
class SoftwareCachedImage : public juce::CachedComponentImage
{
public:
    explicit SoftwareCachedImage (juce::Component& c) : owner (c) {}
    void paint (juce::Graphics& g) override
    {
        const float scale = juce::jmax (1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
        const auto bounds = owner.getLocalBounds();
        const auto imageBounds = (bounds.toFloat() * scale).getSmallestIntegerContainer();
        if (image.isNull() || image.getWidth() != imageBounds.getWidth() || image.getHeight() != imageBounds.getHeight())
        {
            image = juce::Image (juce::Image::RGB, juce::jmax (1, imageBounds.getWidth()), juce::jmax (1, imageBounds.getHeight()), true, juce::NativeImageType());
            valid.clear();
        }
        if (! valid.containsRectangle (bounds))
        {
            juce::RectangleList<int> dirty (bounds);
            for (auto& r : valid) dirty.subtract (r);
            dirty.consolidate();
            // Paint each dirty rectangle on its own. JUCE decides which child components to
            // paint from the clip's bounding box, so one combined clip spanning the display
            // (top left) and the meters (bottom right) would repaint every knob in between.
            juce::LowLevelGraphicsSoftwareRenderer renderer (image);
            renderer.addTransform (juce::AffineTransform::scale (scale));
            juce::Graphics ig (renderer);
            for (auto& r : dirty)
            {
                juce::Graphics::ScopedSaveState state (ig);
                ig.reduceClipRegion (r);
                owner.paintEntireComponent (ig, true);
            }
            valid = bounds;
        }
        g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);     // 1:1 pixels
        g.drawImageTransformed (image, juce::AffineTransform::scale ((float) bounds.getWidth() / (float) image.getWidth(),
                                                                     (float) bounds.getHeight() / (float) image.getHeight()), false);
    }
    bool invalidateAll() override { valid.clear(); pending = owner.getLocalBounds(); return true; }
    bool invalidate (const juce::Rectangle<int>& area) override { valid.subtract (area); pending = pending.isEmpty() ? area : pending.getUnion (area); return true; }
    // For the performance tool: the area the window would be asked to redraw (macOS merges a
    // window's dirty rectangles into one), cleared on read.
    juce::Rectangle<int> takePendingArea() { auto a = pending; pending = {}; return a; }
    void releaseResources() override { image = {}; valid.clear(); }
private:
    juce::Component& owner;
    juce::Image image;
    juce::RectangleList<int> valid;
    juce::Rectangle<int> pending;
};

// ------------------------------------------------------------------ look and feel
class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    explicit LookAndFeel (const Theme& t);
    Theme theme;
    float scale = 1.0f;
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>&, bool sep, bool active, bool highlighted, bool ticked, bool sub,
                            const juce::String& text, const juce::String& shortcut, const juce::Drawable* icon, const juce::Colour* colour) override;
    juce::Font getPopupMenuFont() override { return font (14.0f * scale); }
    void drawTooltip (juce::Graphics&, const juce::String&, int w, int h) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String&, juce::Point<int>, juce::Rectangle<int>) override;
};

// ------------------------------------------------------------------ knob
class Knob : public juce::Component, private juce::Slider::Listener
{
public:
    enum Size { Small, Medium, Large, Hero };
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& label, Size, const juce::String& tip, bool bipolar = false);
    ~Knob() override;
    void resized() override;
    void paint (juce::Graphics&) override;
    juce::Slider slider;
    Size size;
    float scale = 1.0f;
    juce::String label;
private:
    void sliderValueChanged (juce::Slider*) override { repaint(); }
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    juce::RangedAudioParameter* param = nullptr;
    int labelH() const; int valueH() const;
};

// ------------------------------------------------------------------ LED push switch (two-state parameter)
class LedSwitch : public juce::Button
{
public:
    LedSwitch (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& label, const Theme&, const juce::String& tip);
    void paintButton (juce::Graphics&, bool over, bool down) override;
    float scale = 1.0f;
private:
    Theme theme;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

// ------------------------------------------------------------------ segmented selector (choice parameter)
class Segmented : public juce::Component, public juce::SettableTooltipClient
{
public:
    Segmented (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const Theme&, const juce::String& tip, const juce::StringArray& labels = {});
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    float scale = 1.0f;
    juce::String caption;
private:
    int indexAt (juce::Point<float>) const;
    Theme theme;
    juce::StringArray names;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    int current = 0, hover = -1;
};

// ------------------------------------------------------------------ meters
class LedMeter : public juce::Component
{
public:
    explicit LedMeter (const juce::String& label, const Theme& t) : text (label), theme (t) {}
    void push (float linearPeak);
    void paint (juce::Graphics&) override;
    float scale = 1.0f;
private:
    juce::String text; Theme theme;
    float level = -60, hold = -60; int holdTicks = 0;
    int drawnLit = -1, drawnHold = -1;
};

// Analog needle meter. Values in dB; 'reverse' makes it a gain-reduction meter (needle falls to the left).
class NeedleMeter : public juce::Component
{
public:
    NeedleMeter (const juce::String& label, const Theme& t, float minDb, float maxDb, bool reverse, bool creamFace)
        : text (label), theme (t), lo (minDb), hi (maxDb), rev (reverse), cream (creamFace) {}
    void push (float db);
    void paint (juce::Graphics&) override;
    float scale = 1.0f;
private:
    juce::String text; Theme theme; float lo, hi; bool rev, cream;
    float shown = -60, velocity = 0, drawnAngle = 100.0f;
    float angleFor (float db) const;
    juce::Rectangle<int> needleArea (float angle) const;
    void resized() override;
    void renderCache (float pixelScale);
    juce::Image faceImage, glassImage;
    float cacheScale = 0.0f;
};

// ------------------------------------------------------------------ small text button (header)
class PillButton : public juce::Button
{
public:
    PillButton (const juce::String& text, const Theme& t) : juce::Button (text), theme (t) {}
    void paintButton (juce::Graphics&, bool over, bool down) override;
    float scale = 1.0f;
    bool glyphDice = false, glyphPower = false, glyphArrowLeft = false, glyphArrowRight = false;
private:
    Theme theme;
};
}
