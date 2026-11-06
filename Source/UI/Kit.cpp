#include "Kit.h"

namespace eps::ui {
using namespace juce;

// ================================================================== drawing helpers
namespace draw {
static const Image& grainTile()
{
    static Image tile = []
    {
        Image img (Image::ARGB, 256, 256, true);
        Random r (1337);
        for (int y = 0; y < 256; ++y)
            for (int x = 0; x < 256; ++x)
            {
                const float v = r.nextFloat();
                if (v < 0.06f) img.setPixelAt (x, y, Colours::white.withAlpha (0.035f));
                else if (v > 0.94f) img.setPixelAt (x, y, Colours::black.withAlpha (0.09f));
            }
        return img;
    }();
    return tile;
}

void faceplate (Graphics& g, Rectangle<float> r, const Theme& t, float s)
{
    ColourGradient base (t.plateTop, r.getX(), r.getY(), t.plateBottom, r.getX(), r.getBottom(), false);
    base.addColour (0.18, t.plateTop.interpolatedWith (t.plateBottom, 0.25f));
    g.setGradientFill (base);
    g.fillRect (r);
    // brushed metal: fine horizontal streaks
    if (t.brushed)
    {
        Random rnd (77);
        for (float y = r.getY(); y < r.getBottom(); y += 1.0f)
        {
            const float a = 0.012f + 0.022f * rnd.nextFloat();
            g.setColour ((rnd.nextBool() ? Colours::white : Colours::black).withAlpha (a));
            g.fillRect (r.getX(), y, r.getWidth(), 1.0f);
        }
    }
    else
    {
        // soft radial glow in the theme colour (glass/anodised finish)
        ColourGradient glow (t.accent.withAlpha (0.10f), r.getCentreX(), r.getY() + r.getHeight() * 0.25f,
                             Colours::transparentBlack, r.getCentreX(), r.getBottom(), true);
        g.setGradientFill (glow);
        g.fillRect (r);
    }
    g.setTiledImageFill (grainTile(), 0, 0, 1.0f);
    g.fillRect (r);
    // edge lighting
    g.setColour (Colours::white.withAlpha (0.10f));
    g.fillRect (r.getX(), r.getY(), r.getWidth(), 1.0f * s);
    ColourGradient vignette (Colours::transparentBlack, r.getCentreX(), r.getCentreY(), Colours::black.withAlpha (0.35f), r.getX(), r.getY(), true);
    g.setGradientFill (vignette);
    g.fillRect (r);
}

void recess (Graphics& g, Rectangle<float> r, const Theme& t, float radius, float s)
{
    Path p; p.addRoundedRectangle (r, radius);
    g.setColour (t.panel.withAlpha (0.85f));
    g.fillPath (p);
    // inner shadow from the top, light lip at the bottom: an engraved pocket
    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (p);
        ColourGradient sh (Colours::black.withAlpha (0.55f), r.getX(), r.getY(), Colours::transparentBlack, r.getX(), r.getY() + 14 * s, false);
        g.setGradientFill (sh);
        g.fillRect (r);
    }
    g.setColour (Colours::white.withAlpha (0.07f));
    g.drawRoundedRectangle (r.translated (0, 1.0f * s), radius, 1.0f * s);
    g.setColour (Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (r, radius, 1.0f * s);
}

void glass (Graphics& g, Rectangle<float> r, const Theme& t, float radius, float s)
{
    Path p; p.addRoundedRectangle (r, radius);
    // bezel
    g.setColour (Colours::black.withAlpha (0.8f));
    g.fillRoundedRectangle (r.expanded (3 * s), radius + 3 * s);
    ColourGradient bez (Colours::white.withAlpha (0.12f), r.getX(), r.getY() - 3 * s, Colours::white.withAlpha (0.02f), r.getX(), r.getBottom() + 3 * s, false);
    g.setGradientFill (bez);
    g.drawRoundedRectangle (r.expanded (3 * s), radius + 3 * s, 1.2f * s);
    // screen
    ColourGradient bg (t.glass.brighter (0.25f), r.getCentreX(), r.getY(), t.glass, r.getCentreX(), r.getBottom(), false);
    g.setGradientFill (bg);
    g.fillPath (p);
    ColourGradient tint (t.accent.withAlpha (0.07f), r.getCentreX(), r.getCentreY(), Colours::transparentBlack, r.getX(), r.getY(), true);
    g.setGradientFill (tint);
    g.fillPath (p);
}

void screw (Graphics& g, Point<float> c, float r, float angle)
{
    ColourGradient grad (Colour (0xff8d8a86), c.x - r * 0.4f, c.y - r * 0.5f, Colour (0xff2d2c2a), c.x + r * 0.6f, c.y + r * 0.7f, true);
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillEllipse (c.x - r, c.y - r + r * 0.25f, r * 2, r * 2);
    g.setGradientFill (grad);
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    g.setColour (Colour (0xff1b1a19));
    Path slot; slot.addRectangle (-r * 0.75f, -r * 0.12f, r * 1.5f, r * 0.24f);
    g.fillPath (slot, AffineTransform::rotation (angle).translated (c));
}

void engraved (Graphics& g, const String& text, Rectangle<float> r, Justification j, const Font& f, Colour c)
{
    g.setFont (f);
    g.setColour (Colours::black.withAlpha (0.7f));
    g.drawText (text, r.translated (0, 1.0f), j, false);
    g.setColour (c);
    g.drawText (text, r, j, false);
}

void glowStroke (Graphics& g, const Path& p, Colour c, float width, float glow)
{
    for (int i = 3; i >= 1; --i)
    {
        g.setColour (c.withAlpha (0.07f * glow));
        g.strokePath (p, PathStrokeType (width + float (i) * width * 1.6f, PathStrokeType::curved, PathStrokeType::rounded));
    }
    g.setColour (c);
    g.strokePath (p, PathStrokeType (width, PathStrokeType::curved, PathStrokeType::rounded));
}
}

// ================================================================== look and feel
LookAndFeel::LookAndFeel (const Theme& t) : theme (t)
{
    setColour (PopupMenu::backgroundColourId, Colour (0xff15161a));
    setColour (PopupMenu::textColourId, theme.ink);
    setColour (PopupMenu::highlightedBackgroundColourId, theme.accent.withAlpha (0.25f));
    setColour (PopupMenu::highlightedTextColourId, Colours::white);
    setColour (TooltipWindow::textColourId, theme.ink);
}

void LookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, Slider& s)
{
    const float sc = scale;
    const auto bounds = Rectangle<float> (float (x), float (y), float (w), float (h));
    const float radius = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const bool hero = bool (s.getProperties().getWithDefault ("hero", false));
    const bool bipolar = bool (s.getProperties().getWithDefault ("bipolar", false));
    const float angle = a0 + pos * (a1 - a0);
    const float ringR = radius * 0.94f, capR = radius * (hero ? 0.66f : 0.64f);

    // scale ticks
    const int ticks = hero ? 21 : 11;
    for (int i = 0; i < ticks; ++i)
    {
        const float a = a0 + float (i) / float (ticks - 1) * (a1 - a0);
        const bool major = ! hero || i % 2 == 0;
        const float r0 = ringR + 2.5f * sc, r1 = ringR + (major ? 6.5f : 4.5f) * sc;
        g.setColour ((major ? theme.inkDim : theme.inkFaint).withAlpha (0.8f));
        g.drawLine (Line<float> (c.getPointOnCircumference (r0, a), c.getPointOnCircumference (r1, a)), (major ? 1.4f : 1.0f) * sc);
    }
    // groove + value arc with glow
    Path groove; groove.addCentredArc (c.x, c.y, ringR - 3 * sc, ringR - 3 * sc, 0, a0, a1, true);
    g.setColour (Colours::black.withAlpha (0.65f));
    g.strokePath (groove, PathStrokeType (5.0f * sc, PathStrokeType::curved, PathStrokeType::rounded));
    const float from = bipolar ? (a0 + a1) * 0.5f : a0;
    if (std::abs (angle - from) > 0.01f)
    {
        Path arc; arc.addCentredArc (c.x, c.y, ringR - 3 * sc, ringR - 3 * sc, 0, jmin (from, angle), jmax (from, angle), true);
        draw::glowStroke (g, arc, theme.accent, 3.0f * sc, s.isMouseOverOrDragging() ? 1.6f : 1.0f);
    }
    // drop shadow
    for (int i = 6; i >= 1; --i)
    {
        g.setColour (Colours::black.withAlpha (0.07f));
        g.fillEllipse (c.x - capR - float (i) * sc, c.y - capR - float (i) * sc + 4 * sc, (capR + float (i) * sc) * 2, (capR + float (i) * sc) * 2);
    }
    // knurled skirt
    const float skirtR = capR + (hero ? 6.0f : 4.5f) * sc;
    ColourGradient skirt (Colour (0xff4a4744), c.x - skirtR, c.y - skirtR, Colour (0xff121110), c.x + skirtR, c.y + skirtR, false);
    g.setGradientFill (skirt);
    g.fillEllipse (c.x - skirtR, c.y - skirtR, skirtR * 2, skirtR * 2);
    const int knurls = hero ? 72 : 48;
    for (int i = 0; i < knurls; ++i)
    {
        const float a = float (i) / float (knurls) * MathConstants<float>::twoPi + angle;
        g.setColour ((i % 2 ? Colours::white.withAlpha (0.10f) : Colours::black.withAlpha (0.35f)));
        g.drawLine (Line<float> (c.getPointOnCircumference (capR, a), c.getPointOnCircumference (skirtR - 0.5f * sc, a)), 1.0f * sc);
    }
    // machined aluminium cap
    ColourGradient cap (Colour (0xff6d6a66), c.x - capR * 0.6f, c.y - capR * 0.9f, Colour (0xff1f1e1d), c.x + capR * 0.5f, c.y + capR, false);
    cap.addColour (0.45, Colour (0xff3b3936));
    g.setGradientFill (cap);
    g.fillEllipse (c.x - capR, c.y - capR, capR * 2, capR * 2);
    for (int i = 1; i <= 5; ++i)
    {
        const float rr = capR * float (i) / 5.5f;
        g.setColour (Colours::white.withAlpha (i % 2 ? 0.025f : 0.045f));
        g.drawEllipse (c.x - rr, c.y - rr, rr * 2, rr * 2, 0.8f * sc);
    }
    ColourGradient spec (Colours::white.withAlpha (0.22f), c.x - capR * 0.35f, c.y - capR * 0.75f, Colours::transparentWhite, c.x, c.y, true);
    g.setGradientFill (spec);
    g.fillEllipse (c.x - capR, c.y - capR, capR * 2, capR * 2);
    g.setColour (Colours::black.withAlpha (0.6f));
    g.drawEllipse (c.x - capR, c.y - capR, capR * 2, capR * 2, 1.0f * sc);
    // pointer
    Path pointer;
    const auto p0 = c.getPointOnCircumference (capR * 0.30f, angle), p1 = c.getPointOnCircumference (capR * 0.92f, angle);
    pointer.startNewSubPath (p0); pointer.lineTo (p1);
    draw::glowStroke (g, pointer, theme.accent.brighter (0.3f), (hero ? 3.2f : 2.4f) * sc, 1.2f);
}

void LookAndFeel::drawPopupMenuBackground (Graphics& g, int w, int h)
{
    g.fillAll (Colour (0xff141518));
    g.setColour (theme.accent.withAlpha (0.35f));
    g.drawRect (0, 0, w, h, 1);
}
void LookAndFeel::drawPopupMenuItem (Graphics& g, const Rectangle<int>& area, bool sep, bool active, bool highlighted, bool ticked, bool, const String& text,
                                     const String&, const Drawable*, const Colour*)
{
    if (sep) { g.setColour (Colours::white.withAlpha (0.08f)); g.fillRect (area.reduced (8, 0).withHeight (1).withY (area.getCentreY())); return; }
    auto r = area.toFloat().reduced (4, 1);
    if (highlighted && active) { g.setColour (theme.accent.withAlpha (0.22f)); g.fillRoundedRectangle (r, 4.0f); }
    g.setColour (ticked ? theme.accent : (active ? theme.ink : theme.inkFaint));
    g.setFont (font (14.0f * scale, ticked));
    g.drawText (text, r.withTrimmedLeft (14.0f * scale), Justification::centredLeft);
    if (ticked) { g.fillEllipse (r.getX() + 4 * scale, r.getCentreY() - 2.5f * scale, 5 * scale, 5 * scale); }
}
void LookAndFeel::drawTooltip (Graphics& g, const String& text, int w, int h)
{
    g.setColour (Colour (0xf0101114)); g.fillRoundedRectangle (0, 0, float (w), float (h), 5.0f);
    g.setColour (theme.accent.withAlpha (0.5f)); g.drawRoundedRectangle (0.5f, 0.5f, float (w) - 1, float (h) - 1, 5.0f, 1.0f);
    g.setColour (theme.ink); g.setFont (font (13.0f * scale));
    g.drawFittedText (text, Rectangle<int> (10, 6, w - 20, h - 12), Justification::centredLeft, 4);
}
Rectangle<int> LookAndFeel::getTooltipBounds (const String& text, Point<int> pos, Rectangle<int> area)
{
    const int w = jmin (int (320 * scale), int (GlyphArrangement::getStringWidth (font (13.0f * scale), text)) + int (24 * scale));
    const int lines = 1 + int (GlyphArrangement::getStringWidth (font (13.0f * scale), text)) / jmax (1, w - int (24 * scale));
    const int h = int ((12 + 17 * lines) * scale);
    return Rectangle<int> (pos.x + 12, pos.y + 16, w, h).constrainedWithin (area);
}

// ================================================================== knob
Knob::Knob (AudioProcessorValueTreeState& st, const String& id, const String& text, Size sz, const String& tip, bool bipolar)
    : size (sz), label (text.toUpperCase())
{
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (MathConstants<float>::pi * 1.22f, MathConstants<float>::pi * 2.78f, true);
    slider.setMouseDragSensitivity (sz == Hero ? 300 : 220);
    slider.setVelocityModeParameters (0.6, 1, 0.02, true, ModifierKeys::shiftModifier);
    slider.getProperties().set ("hero", sz == Hero);
    slider.getProperties().set ("bipolar", bipolar);
    slider.setTooltip (tip);
    slider.addListener (this);
    addAndMakeVisible (slider);
    attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (st, id, slider);
    param = st.getParameter (id);
    if (param)
    {
        slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
        auto* p = param;
        slider.textFromValueFunction = [p] (double v) { return p->getText (p->convertTo0to1 (float (v)), 32); };
    }
    setTitle (text);
}
Knob::~Knob() { slider.removeListener (this); }
int Knob::labelH() const { return roundToInt ((size == Hero ? 20.0f : 16.0f) * scale); }
int Knob::valueH() const { return roundToInt ((size == Hero ? 24.0f : 19.0f) * scale); }
void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (labelH()); r.removeFromBottom (valueH() + roundToInt (3 * scale));
    const int side = jmin (r.getWidth(), r.getHeight());
    slider.setBounds (r.withSizeKeepingCentre (side, side));
}
void Knob::paint (Graphics& g)
{
    auto& lf = dynamic_cast<LookAndFeel&> (getLookAndFeel());
    const auto& t = lf.theme;
    auto r = getLocalBounds().toFloat();
    draw::engraved (g, label, r.removeFromTop (float (labelH())), Justification::centred,
                    font ((size == Hero ? 12.5f : 10.5f) * scale, true, 0.16f), slider.isMouseOverOrDragging() ? t.ink : t.inkDim);
    // value readout in a small LCD
    auto v = r.removeFromBottom (float (valueH()));
    const String txt = slider.getTextFromValue (slider.getValue());
    const auto f = font ((size == Hero ? 14.5f : 12.0f) * scale, true);
    const float tw = GlyphArrangement::getStringWidth (f, txt) + 16 * scale;
    auto pill = v.withSizeKeepingCentre (jmin (v.getWidth(), jmax (tw, 54 * scale)), v.getHeight());
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (pill, pill.getHeight() * 0.5f);
    g.setColour (Colours::white.withAlpha (0.05f));
    g.drawRoundedRectangle (pill.translated (0, 1), pill.getHeight() * 0.5f, 1.0f);
    g.setColour (slider.isMouseOverOrDragging() ? t.accent.brighter (0.2f) : t.accent.withAlpha (0.9f));
    g.setFont (f);
    g.drawText (txt, pill, Justification::centred);
}

// ================================================================== LED switch
LedSwitch::LedSwitch (AudioProcessorValueTreeState& st, const String& id, const String& text, const Theme& t, const String& tip)
    : Button (text), theme (t)
{
    setClickingTogglesState (true);
    setTooltip (tip);
    attachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (st, id, *this);
}
void LedSwitch::paintButton (Graphics& g, bool over, bool down)
{
    const float s = scale;
    auto r = getLocalBounds().toFloat().reduced (1.5f * s);
    const bool on = getToggleState();
    // body
    ColourGradient body (Colour (0xff34322f).brighter (over ? 0.08f : 0.0f), r.getX(), r.getY(), Colour (0xff141312), r.getX(), r.getBottom(), false);
    g.setColour (Colours::black.withAlpha (0.6f)); g.fillRoundedRectangle (r.translated (0, 2 * s), 6 * s);
    g.setGradientFill (body); g.fillRoundedRectangle (r.translated (0, down ? 1.0f * s : 0.0f), 6 * s);
    g.setColour (Colours::white.withAlpha (0.10f)); g.drawRoundedRectangle (r.reduced (0.5f), 6 * s, 1.0f);
    // LED
    const float d = 7.5f * s;
    const auto led = Rectangle<float> (r.getX() + 10 * s, r.getCentreY() - d * 0.5f, d, d);
    if (on)
    {
        for (int i = 3; i >= 1; --i) { g.setColour (theme.accent.withAlpha (0.12f)); g.fillEllipse (led.expanded (float (i) * 2.2f * s)); }
        ColourGradient lg (Colours::white, led.getCentreX() - d * 0.2f, led.getCentreY() - d * 0.2f, theme.accent, led.getRight(), led.getBottom(), true);
        g.setGradientFill (lg);
    }
    else g.setColour (Colour (0xff201e1c));
    g.fillEllipse (led);
    g.setColour (Colours::black.withAlpha (0.7f)); g.drawEllipse (led, 0.8f * s);
    g.setFont (font (11.0f * s, true, 0.14f));
    g.setColour (on ? theme.ink : theme.inkDim);
    g.drawText (getButtonText().toUpperCase(), r.withTrimmedLeft (24 * s).withTrimmedRight (6 * s), Justification::centredLeft);
}

// ================================================================== segmented selector
Segmented::Segmented (AudioProcessorValueTreeState& st, const String& id, const Theme& t, const String& tip, const StringArray& labels)
    : theme (t)
{
    param = st.getParameter (id);
    if (labels.isEmpty() && param)
        for (int i = 0; i <= roundToInt (param->getNormalisableRange().end); ++i) names.add (param->getText (param->convertTo0to1 (float (i)), 32).replace (" dotted", "D"));
    else names = labels;
    setTooltip (tip);
    if (param)
    {
        attachment = std::make_unique<ParameterAttachment> (*param, [this] (float v) { current = roundToInt (v); repaint(); });
        attachment->sendInitialUpdate();
    }
}
int Segmented::indexAt (Point<float> p) const
{
    const float w = float (getWidth()) / float (jmax (1, names.size()));
    return jlimit (0, names.size() - 1, int (p.x / w));
}
void Segmented::mouseDown (const MouseEvent& e) { if (attachment) attachment->setValueAsCompleteGesture (float (indexAt (e.position))); }
void Segmented::mouseMove (const MouseEvent& e) { const int h = indexAt (e.position); if (h != hover) { hover = h; repaint(); } }
void Segmented::mouseExit (const MouseEvent&) { hover = -1; repaint(); }
void Segmented::paint (Graphics& g)
{
    const float s = scale;
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (Colours::black.withAlpha (0.65f)); g.fillRoundedRectangle (r, 6 * s);
    g.setColour (Colours::white.withAlpha (0.06f)); g.drawRoundedRectangle (r.translated (0, 1), 6 * s, 1.0f);
    const int n = jmax (1, names.size());
    const float w = r.getWidth() / float (n);
    for (int i = 0; i < n; ++i)
    {
        auto cell = Rectangle<float> (r.getX() + w * float (i), r.getY(), w, r.getHeight()).reduced (2.0f * s);
        if (i == current)
        {
            for (int k = 2; k >= 1; --k) { g.setColour (theme.accent.withAlpha (0.10f)); g.fillRoundedRectangle (cell.expanded (float (k) * 2 * s), 5 * s); }
            ColourGradient on (theme.accent.brighter (0.25f), cell.getX(), cell.getY(), theme.accent.darker (0.35f), cell.getX(), cell.getBottom(), false);
            g.setGradientFill (on); g.fillRoundedRectangle (cell, 5 * s);
            g.setColour (Colours::white.withAlpha (0.35f)); g.drawLine (cell.getX() + 4 * s, cell.getY() + 1, cell.getRight() - 4 * s, cell.getY() + 1, 1.0f);
        }
        else if (i == hover) { g.setColour (Colours::white.withAlpha (0.06f)); g.fillRoundedRectangle (cell, 5 * s); }
        if (i > 0 && i != current && i - 1 != current)
        { g.setColour (Colours::white.withAlpha (0.07f)); g.fillRect (cell.getX() - 2 * s, cell.getY() + cell.getHeight() * 0.25f, 1.0f, cell.getHeight() * 0.5f); }
        g.setFont (font (jmin (11.5f * s, cell.getHeight() * 0.48f), true, 0.08f));
        g.setColour (i == current ? Colour (0xff0d0c0b) : theme.inkDim);
        g.drawFittedText (names[i].toUpperCase(), cell.toNearestInt(), Justification::centred, 1, 0.75f);
    }
}

// ================================================================== meters
void LedMeter::push (float linearPeak)
{
    const float db = Decibels::gainToDecibels (linearPeak, -60.0f);
    level = db > level ? db : jmax (db, level - 1.5f);
    if (db >= hold) { hold = db; holdTicks = 45; } else if (--holdTicks <= 0) hold = jmax (-60.0f, hold - 1.0f);
    repaint();
}
void LedMeter::paint (Graphics& g)
{
    const float s = scale;
    auto r = getLocalBounds().toFloat();
    draw::engraved (g, text, r.removeFromBottom (14 * s), Justification::centred, font (9.0f * s, true, 0.14f), theme.inkDim);
    r = r.withSizeKeepingCentre (jmin (r.getWidth(), 9.0f * s), r.getHeight() - 2 * s);
    g.setColour (Colours::black.withAlpha (0.7f)); g.fillRoundedRectangle (r.expanded (2 * s), 3 * s);
    const int segs = 24;
    const float gap = 1.5f * s, h = (r.getHeight() - gap * float (segs - 1)) / float (segs);
    for (int i = 0; i < segs; ++i)
    {
        const float dbAt = -60.0f + 63.0f * float (i + 1) / float (segs);
        const auto seg = Rectangle<float> (r.getX(), r.getBottom() - float (i + 1) * (h + gap) + gap, r.getWidth(), h);
        const Colour c = dbAt > -0.5f ? Colour (0xffff4a4a) : dbAt > -9.0f ? Colour (0xffffc04d) : theme.accent;
        const bool lit = dbAt <= level + 1.0f || std::abs (dbAt - hold) < 1.4f;
        g.setColour (lit ? c : c.withAlpha (0.10f));
        g.fillRoundedRectangle (seg, 1.0f * s);
    }
}

void NeedleMeter::push (float db)
{
    db = jlimit (lo, hi, std::isfinite (db) ? db : lo);
    // ballistics: a damped mass-spring, like a real VU movement
    const float force = (db - shown) * 0.14f - velocity * 0.45f;
    velocity += force; shown += velocity;
    const float a = angleFor (shown);
    if (std::abs (a - drawnAngle) > 0.0005f) repaint();      // a resting needle costs nothing
}
float NeedleMeter::angleFor (float db) const
{
    const float t = jlimit (0.0f, 1.0f, (db - lo) / (hi - lo));
    const float u = rev ? 1.0f - t : t;
    return -0.85f + 1.7f * u;   // radians from vertical
}
void NeedleMeter::resized() { cacheScale = 0.0f; }

// The face (scale, numbers, lamp glow) and the glass (reflection, bezel) never change, so they are
// drawn once into bitmaps at the screen's pixel density; each frame only the needle is drawn.
void NeedleMeter::renderCache (float px)
{
    cacheScale = px;
    const int W = jmax (1, roundToInt ((float) getWidth() * px)), H = jmax (1, roundToInt ((float) getHeight() * px));
    faceImage = Image (Image::ARGB, W, H, true);
    glassImage = Image (Image::ARGB, W, H, true);
    const float s = scale;
    auto r = getLocalBounds().toFloat();
    Path face; face.addRoundedRectangle (r, 8 * s);
    {
        Graphics g (faceImage);
        g.addTransform (AffineTransform::scale (px));
        if (cream)
            g.setGradientFill (ColourGradient (Colour (0xfff3e3bd), r.getCentreX(), r.getY(), Colour (0xffd9be86), r.getCentreX(), r.getBottom(), false));
        else
            g.setGradientFill (ColourGradient (Colour (0xff1d1c1b), r.getCentreX(), r.getY(), Colour (0xff0b0a0a), r.getCentreX(), r.getBottom(), false));
        g.fillPath (face);
        ColourGradient lamp (theme.accent.withAlpha (cream ? 0.18f : 0.16f), r.getCentreX(), r.getBottom(), Colours::transparentBlack, r.getCentreX(), r.getY(), true);
        g.setGradientFill (lamp); g.fillPath (face);
        const Colour ink = cream ? Colour (0xff2a2116) : theme.inkDim;
        const auto pivot = Point<float> (r.getCentreX(), r.getBottom() + r.getHeight() * 0.18f);
        const float R = r.getHeight() * 0.98f;
        Path arc; arc.addCentredArc (pivot.x, pivot.y, R * 0.82f, R * 0.82f, 0, -0.85f, 0.85f, true);
        g.setColour (ink.withAlpha (0.8f)); g.strokePath (arc, PathStrokeType (1.2f * s));
        const float redFrom = rev ? lo + (hi - lo) * 0.7f : hi - (hi - lo) * 0.18f;
        Path red; red.addCentredArc (pivot.x, pivot.y, R * 0.80f, R * 0.80f, 0, jmin (angleFor (redFrom), angleFor (hi)), jmax (angleFor (redFrom), angleFor (hi)), true);
        g.setColour (Colour (0xffd6332b).withAlpha (0.9f)); g.strokePath (red, PathStrokeType (3.5f * s));
        g.setFont (font (9.5f * s, true));
        for (int i = 0; i <= 6; ++i)
        {
            const float db = lo + (hi - lo) * float (i) / 6.0f, a = angleFor (db);
            g.setColour (ink);
            g.drawLine (Line<float> (pivot.getPointOnCircumference (R * 0.82f, a), pivot.getPointOnCircumference (R * 0.88f, a)), 1.2f * s);
            const auto tp = pivot.getPointOnCircumference (R * 0.96f, a);
            g.drawText (String (roundToInt (std::abs (db))), Rectangle<float> (tp.x - 14 * s, tp.y - 7 * s, 28 * s, 14 * s), Justification::centred);
        }
        g.setFont (font (11.0f * s, true, 0.2f));
        g.setColour (ink.withAlpha (0.9f));
        g.drawText (text, r.withTrimmedTop (r.getHeight() * 0.52f).withHeight (16 * s), Justification::centred);
    }
    {
        Graphics g (glassImage);
        g.addTransform (AffineTransform::scale (px));
        ColourGradient refl (Colours::white.withAlpha (0.12f), r.getX(), r.getY(), Colours::transparentWhite, r.getX(), r.getCentreY(), false);
        g.setGradientFill (refl); g.fillPath (face);
        g.setColour (Colours::black.withAlpha (0.8f)); g.strokePath (face, PathStrokeType (2.0f * s));
    }
}

void NeedleMeter::paint (Graphics& g)
{
    const float px = jmax (1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    if (std::abs (px - cacheScale) > 0.01f || ! faceImage.isValid()) renderCache (px);
    const auto r = getLocalBounds().toFloat();
    g.setImageResamplingQuality (Graphics::lowResamplingQuality);
    g.drawImage (faceImage, r);
    const float s = scale;
    const auto pivot = Point<float> (r.getCentreX(), r.getBottom() + r.getHeight() * 0.18f);
    const float R = r.getHeight() * 0.98f;
    const float a = angleFor (shown);
    drawnAngle = a;
    const auto tip = pivot.getPointOnCircumference (R * 0.9f, a), base = pivot.getPointOnCircumference (R * 0.25f, a);
    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (r.reduced (2 * s).toNearestInt());   // the needle base sits below the face: keep it inside
        g.setColour (Colours::black.withAlpha (0.25f)); g.drawLine (Line<float> (base.translated (3 * s, 3 * s), tip.translated (3 * s, 3 * s)), 2.0f * s);
        g.setColour (cream ? Colour (0xff1b1712) : theme.accent); g.drawLine (Line<float> (base, tip), 1.6f * s);
    }
    g.drawImage (glassImage, r);
}

// ================================================================== header pill button
void PillButton::paintButton (Graphics& g, bool over, bool down)
{
    const float s = scale;
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const bool on = getToggleState();
    g.setColour (Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    if (on || over) { g.setColour ((on ? theme.accent.withAlpha (0.22f) : Colours::white.withAlpha (0.05f))); g.fillRoundedRectangle (r, r.getHeight() * 0.5f); }
    g.setColour (on ? theme.accent.withAlpha (0.8f) : Colours::white.withAlpha (0.12f)); g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
    const Colour ink = on ? theme.accent.brighter (0.3f) : (over ? theme.ink : theme.inkDim);
    g.setColour (ink);
    if (glyphPower)
    {
        const auto c = r.getCentre(); const float rad = r.getHeight() * 0.24f;
        Path p; p.addCentredArc (c.x, c.y, rad, rad, 0, 0.7f, MathConstants<float>::twoPi - 0.7f, true);
        p.startNewSubPath (c.x, c.y - rad * 1.3f); p.lineTo (c.x, c.y - rad * 0.2f);
        g.setColour (on ? Colour (0xffff5a52) : theme.accent);
        g.strokePath (p, PathStrokeType (1.8f * s, PathStrokeType::curved, PathStrokeType::rounded));
        return;
    }
    if (glyphArrowLeft || glyphArrowRight)
    {
        const auto c = r.getCentre(); const float k = r.getHeight() * 0.16f;
        Path p; if (glyphArrowLeft) p.addTriangle (c.x + k, c.y - k * 1.3f, c.x + k, c.y + k * 1.3f, c.x - k, c.y); else p.addTriangle (c.x - k, c.y - k * 1.3f, c.x - k, c.y + k * 1.3f, c.x + k, c.y);
        g.fillPath (p); return;
    }
    auto text = r;
    if (glyphDice)
    {
        auto d = r.removeFromLeft (r.getHeight()).reduced (r.getHeight() * 0.26f).translated (6 * s, 0);
        g.drawRoundedRectangle (d, 2.0f * s, 1.3f * s);
        for (auto p : { d.getCentre(), d.getTopLeft() + Point<float> (d.getWidth() * 0.27f, d.getHeight() * 0.27f), d.getBottomRight() - Point<float> (d.getWidth() * 0.27f, d.getHeight() * 0.27f) })
            g.fillEllipse (p.x - 1.3f * s, p.y - 1.3f * s, 2.6f * s, 2.6f * s);
        text = r.translated (4 * s, 0);
    }
    g.setFont (font (11.0f * s, true, 0.12f));
    g.drawText (getButtonText().toUpperCase(), text.translated (0, down ? 1.0f : 0.0f), Justification::centred);
}
}
