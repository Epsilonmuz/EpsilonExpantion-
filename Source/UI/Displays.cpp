#include "Displays.h"
#include <complex>
#include "../DSP/ColourProcessors.h"
#include "../DSP/Reverbs.h"

namespace eps::ui {
using namespace juce;

void Display::grid (Graphics& g, Rectangle<float> r, int cols, int rows) const
{
    g.setColour (theme.accent.withAlpha (0.06f));
    for (int i = 1; i < cols; ++i) g.drawVerticalLine (roundToInt (r.getX() + r.getWidth() * float (i) / float (cols)), r.getY(), r.getBottom());
    for (int i = 1; i < rows; ++i) g.drawHorizontalLine (roundToInt (r.getY() + r.getHeight() * float (i) / float (rows)), r.getX(), r.getRight());
}
void Display::caption (Graphics& g, Rectangle<float> r, const String& left, const String& right) const
{
    g.setFont (font (10.5f * scale, true, 0.16f));
    g.setColour (theme.accent.withAlpha (0.85f));
    g.drawText (left, r.reduced (12 * scale, 8 * scale), Justification::topLeft);
    g.setColour (theme.inkDim);
    g.drawText (right, r.reduced (12 * scale, 8 * scale), Justification::topRight);
}
void Display::scopeTrace (Graphics& g, Rectangle<float> r, float alpha) const
{
    const unsigned w = proc.scopeWrite.load();
    Path p;
    for (int i = 0; i < 256; ++i)
    {
        const float v = proc.scope[(w - 256 + unsigned (i)) & 511u].load();
        const float x = r.getX() + r.getWidth() * float (i) / 255.0f, y = r.getCentreY() - jlimit (-1.0f, 1.0f, v) * r.getHeight() * 0.45f;
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    g.setColour (theme.accent2.withAlpha (alpha));
    g.strokePath (p, PathStrokeType (1.2f * scale));
}

namespace {
// ------------------------------------------------------------------ HEAT: live transfer curve
class HeatDisplay : public Display
{
public:
    using Display::Display;
    void paint (Graphics& g) override
    {
        const auto v = proc.values();
        auto r = screen();
        const int style = jlimit (0, 4, roundToInt (v[heat::style]));
        static const char* names[] = { "TAPE", "CONSOLE", "GERMANIUM", "TRIODE", "PENTODE" };
        caption (g, r, String ("STYLE  ") + names[style], v[heat::punish] > 0.5 ? "PUNISH +20 dB" : "");
        auto plot = r.reduced (18 * scale, 30 * scale).withTrimmedRight (40 * scale);
        grid (g, plot, 8, 6);
        // watermark
        g.setFont (font (64.0f * scale, true, 0.1f)); g.setColour (theme.accent.withAlpha (0.045f));
        g.drawText (names[style], r, Justification::centred);
        const double drive = gain (v[heat::drive] + (v[heat::punish] > 0.5 ? 20 : 0)) * 0.5;
        auto curveAt = [&] (double x) { return curves::byStyle (style, x * drive) / std::max (1.0, std::abs (curves::byStyle (style, 1.6 * drive))); };
        Path p;
        for (int i = 0; i <= 200; ++i)
        {
            const double x = -1.6 + 3.2 * i / 200.0;
            const float px = plot.getX() + plot.getWidth() * float (i) / 200.0f, py = plot.getCentreY() - float (curveAt (x)) * plot.getHeight() * 0.48f;
            if (i == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
        }
        g.setColour (theme.inkFaint.withAlpha (0.5f));
        g.drawLine (plot.getX(), plot.getBottom(), plot.getRight(), plot.getY(), 1.0f);
        draw::glowStroke (g, p, theme.accent, 2.2f * scale, 1.3f);
        // live operating point from the attitude meter
        const float lvl = proc.driveLevel.load();
        const double xin = std::min (1.6, gain (double (lvl)) / std::max (1e-6, drive) * 2.0 * 0.5);
        const float px = plot.getX() + plot.getWidth() * float ((xin + 1.6) / 3.2), py = plot.getCentreY() - float (curveAt (xin)) * plot.getHeight() * 0.48f;
        for (int i = 3; i >= 1; --i) { g.setColour (theme.accent2.withAlpha (0.15f)); g.fillEllipse (px - float (i) * 4 * scale, py - float (i) * 4 * scale, float (i) * 8 * scale, float (i) * 8 * scale); }
        g.setColour (Colours::white); g.fillEllipse (px - 3 * scale, py - 3 * scale, 6 * scale, 6 * scale);
        // attitude bar
        auto bar = r.withTrimmedLeft (r.getWidth() - 34 * scale).reduced (10 * scale, 32 * scale);
        g.setColour (Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (bar, 3 * scale);
        const float fill = jlimit (0.0f, 1.0f, (lvl + 30.0f) / 36.0f);
        ColourGradient cg (theme.accent, bar.getX(), bar.getBottom(), Colour (0xffff4a3a), bar.getX(), bar.getY(), false);
        g.setGradientFill (cg); g.fillRoundedRectangle (bar.withTop (bar.getBottom() - bar.getHeight() * fill), 3 * scale);
        g.setFont (font (8.5f * scale, true, 0.2f)); g.setColour (theme.inkDim);
        g.drawText ("ATT", bar.withY (bar.getBottom() + 3 * scale).withHeight (12 * scale).expanded (6 * scale, 0), Justification::centred);
    }
};

// ------------------------------------------------------------------ VALVE / CRUSH: analog meters
class MeterDisplay : public Display
{
public:
    MeterDisplay (ExpansionProcessor& p, const Theme& t, bool gr) : Display (p, t), isGr (gr),
        meter (gr ? "GAIN REDUCTION" : "HEAT", t, gr ? 0.0f : -20.0f, gr ? 20.0f : 3.0f, gr, ! gr)
    { addAndMakeVisible (meter); }
    void resized() override { meter.scale = scale; meter.setBounds (screen().reduced (16 * scale, 14 * scale).toNearestInt()); }
    void tick() override
    {
        meter.push (isGr ? proc.reduction.load() : proc.driveLevel.load());   // the needle repaints its own sweep
        const bool lock = isGr && proc.reduction.load() > 6.0f;
        if (lock != lockShown) { lockShown = lock; repaint(); }
    }
    void paintOverChildren (Graphics& g) override
    {
        if (isGr && proc.reduction.load() > 6.0f)
        {
            auto lamp = screen().reduced (14 * scale).removeFromTop (22 * scale).removeFromRight (64 * scale);
            g.setColour (theme.accent.withAlpha (0.25f)); g.fillRoundedRectangle (lamp.expanded (3 * scale), 6 * scale);
            g.setColour (theme.accent); g.fillRoundedRectangle (lamp, 4 * scale);
            g.setColour (Colours::black); g.setFont (font (10.0f * scale, true, 0.2f)); g.drawText ("LOCK", lamp, Justification::centred);
        }
    }
private:
    bool isGr, lockShown = false;
    NeedleMeter meter;
};

// ------------------------------------------------------------------ PRISM: splices
class PrismDisplay : public Display
{
public:
    using Display::Display;
    void tick() override { phase += 1.0 / 30.0; repaint(); }
    void paint (Graphics& g) override
    {
        const auto v = proc.values();
        auto r = screen();
        const double spliceS = (v[prism::sync] > 0.5 ? 60.0 / proc.bpmDisplay.load() * beats (v[prism::note]) * 1000 : v[prism::splice]) * 0.001;
        const bool rev = v[prism::reverse] > 0.5;
        caption (g, r, rev ? "REVERSE SPLICES" : "FORWARD SPLICES", String (spliceS * 1000, 0) + " ms");
        auto plot = r.reduced (18 * scale, 30 * scale);
        grid (g, plot, 12, 4);
        scopeTrace (g, plot, 0.18f);
        const double window = 3.0;                         // seconds across the screen
        const double ratio = std::exp2 (v[prism::pitch] / 12.0);
        const float h = plot.getHeight() * float (jlimit (0.15, 0.95, 0.35 + 0.25 * std::log2 (ratio)));
        const double start = std::fmod (phase, spliceS);
        for (double t = -start; t < window; t += spliceS)
        {
            const float x0 = plot.getX() + plot.getWidth() * float (t / window), x1 = plot.getX() + plot.getWidth() * float ((t + spliceS) / window);
            if (x1 < plot.getX()) continue;
            Path ramp;
            const float yb = plot.getBottom(), yt = plot.getBottom() - h;
            if (rev) { ramp.startNewSubPath (x0, yt); ramp.lineTo (x1, yb); } else { ramp.startNewSubPath (x0, yb); ramp.lineTo (x1, yt); }
            Path fill (ramp); fill.lineTo (x1, yb); fill.lineTo (x0, yb); fill.closeSubPath();
            g.setGradientFill (ColourGradient (theme.accent.withAlpha (0.28f), x0, yt, theme.accent2.withAlpha (0.04f), x0, yb, false));
            g.fillPath (fill);
            draw::glowStroke (g, ramp, theme.accent, 1.8f * scale, 1.0f);
        }
        g.setFont (font (34.0f * scale, true)); g.setColour (theme.ink.withAlpha (0.92f));
        g.drawText ((v[prism::pitch] > 0 ? "+" : "") + String (v[prism::pitch], 1), plot.withTrimmedBottom (plot.getHeight() * 0.35f), Justification::topRight);
        g.setFont (font (10.0f * scale, true, 0.2f)); g.setColour (theme.inkDim);
        g.drawText ("SEMITONES", plot.withTrimmedTop (40 * scale), Justification::topRight);
    }
private:
    double phase = 0;
};

// ------------------------------------------------------------------ TIME: echo timeline
class TimeDisplay : public Display
{
public:
    using Display::Display;
    void paint (Graphics& g) override
    {
        const auto v = proc.values();
        auto r = screen();
        const int mult = 1 << jlimit (0, 3, roundToInt (v[time::multiply]));
        const double bpm = proc.bpmDisplay.load();
        double tA = v[time::sync] > 0.5 ? 60000.0 / bpm * beats (v[time::noteA]) : v[time::timeA];
        double tB = v[time::link] > 0.5 ? tA * 1.5 : (v[time::sync] > 0.5 ? 60000.0 / bpm * beats (v[time::noteB]) : v[time::timeB]);
        tA *= mult; tB *= mult;
        static const char* modes[] = { "CLASSIC", "PING-PONG", "SERIES", "CROSS" };
        caption (g, r, String (modes[jlimit (0, 3, roundToInt (v[time::mode]))]) + "   " + String (mult) + "X  CLOCK",
                 v[time::freeze] > 0.5 ? "FROZEN" : String (12.0 / mult, mult > 4 ? 1 : 0) + " kHz BANDWIDTH");
        auto plot = r.reduced (18 * scale, 30 * scale);
        grid (g, plot, 10, 4);
        scopeTrace (g, plot, 0.15f);
        const double span = std::max (tA, tB) * 5.0;
        auto lane = [&] (double t, double fb, double lvl, Colour c, float yMid, const String& name)
        {
            double a = lvl;
            for (int k = 1; k <= 24; ++k)
            {
                const double x = t * k;
                if (x > span || a < 0.02) break;
                const float px = plot.getX() + plot.getWidth() * float (x / span), hh = plot.getHeight() * 0.42f * float (std::min (1.0, a));
                Path p; p.startNewSubPath (px, yMid); p.lineTo (px, yMid - hh);
                draw::glowStroke (g, p, c, 2.2f * scale, 0.9f);
                a *= std::min (fb, 1.0);
            }
            g.setFont (font (11.0f * scale, true, 0.1f)); g.setColour (c);
            g.drawText (name + "  " + (t >= 1000 ? String (t / 1000.0, 2) + " s" : String (roundToInt (t)) + " ms"), Rectangle<float> (plot.getX(), yMid - 16 * scale, 200 * scale, 14 * scale), Justification::left);
        };
        const float yA = plot.getY() + plot.getHeight() * 0.48f, yB = plot.getBottom();
        g.setColour (theme.inkFaint.withAlpha (0.4f)); g.drawHorizontalLine (roundToInt (yA), plot.getX(), plot.getRight()); g.drawHorizontalLine (roundToInt (yB), plot.getX(), plot.getRight());
        lane (tA, v[time::feedbackA], v[time::levelA], theme.accent, yA, "A");
        lane (tB, v[time::feedbackB], v[time::levelB], theme.accent2, yB, "B");
    }
};

// ------------------------------------------------------------------ PLATE: decay envelope over the plate
class PlateDisplay : public Display
{
public:
    using Display::Display;
    void tick() override { energy = 0.85f * energy + 0.15f * proc.outputPeak.load(); phase += 0.05; repaint(); }
    void paint (Graphics& g) override
    {
        const auto v = proc.values();
        auto r = screen();
        static const char* models[] = { "STEEL", "GOLD FOIL", "BRIGHT", "WARM", "SPLASH" };
        caption (g, r, String ("PLATE  ") + models[jlimit (0, 4, roundToInt (v[plate::model]))], v[plate::freeze] > 0.5 ? "FROZEN" : "RT60  " + String (v[plate::decay], 2) + " s");
        auto plot = r.reduced (20 * scale, 34 * scale);
        // the suspended plate
        auto plateR = plot.withTrimmedBottom (plot.getHeight() * 0.45f).reduced (plot.getWidth() * 0.12f, 6 * scale);
        for (auto c : { plateR.getTopLeft(), plateR.getTopRight(), plateR.getBottomLeft(), plateR.getBottomRight() })
        {
            const auto anchor = c + Point<float> ((c.x < plateR.getCentreX() ? -1.0f : 1.0f) * 14 * scale, (c.y < plateR.getCentreY() ? -1.0f : 1.0f) * 8 * scale);
            g.setColour (theme.inkFaint); g.drawLine (Line<float> (anchor, c), 1.0f * scale);
        }
        ColourGradient pg (theme.accent.withAlpha (0.28f), plateR.getX(), plateR.getY(), theme.accent2.withAlpha (0.08f), plateR.getRight(), plateR.getBottom(), false);
        g.setGradientFill (pg); g.fillRect (plateR);
        const float shimmer = jlimit (0.0f, 1.0f, energy * 3.0f);
        for (int i = 0; i < 26; ++i)
        {
            const float x = plateR.getX() + plateR.getWidth() * float (i) / 25.0f;
            const float wob = std::sin (float (phase) * 3.0f + float (i) * 0.7f) * shimmer * 4.0f * scale;
            g.setColour (theme.accent2.withAlpha (0.06f + 0.2f * shimmer));
            g.drawLine (x + wob, plateR.getY(), x - wob, plateR.getBottom(), 1.0f);
        }
        g.setColour (theme.accent.withAlpha (0.7f)); g.drawRect (plateR, 1.0f);
        // decay envelope
        auto env = plot.withTrimmedTop (plot.getHeight() * 0.62f);
        grid (g, env, 10, 2);
        Path p; const double span = std::max (1.0, std::min (40.0, v[plate::decay] * 1.4));
        for (int i = 0; i <= 120; ++i)
        {
            const double t = span * i / 120.0;
            const double pd = v[plate::predelay] * 0.001;
            double a = t < pd ? 0.0 : std::pow (10.0, -3.0 * (t - pd) / std::max (0.1, v[plate::decay])) ;
            if (v[plate::freeze] > 0.5) a = t < pd ? 0 : 1;
            const float x = env.getX() + env.getWidth() * float (i) / 120.0f, y = env.getBottom() - env.getHeight() * float (std::sqrt (a));
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        draw::glowStroke (g, p, theme.accent, 2.0f * scale, 1.0f);
        g.setFont (font (9.5f * scale, true, 0.15f)); g.setColour (theme.inkDim);
        g.drawText (String (span, 1) + " s", env, Justification::bottomRight);
    }
private:
    float energy = 0; double phase = 0;
};

// ------------------------------------------------------------------ SPACE: envelope across the time window, with a draggable shape point
class SpaceDisplay : public Display
{
public:
    using Display::Display;
    void paint (Graphics& g) override
    {
        const auto v = proc.values();
        auto r = screen();
        static const double beatsTable[6] = { 1, 2, 4, 8, 16, 32 };
        const double T = v[space::sync] > 0.5 ? 60.0 / proc.bpmDisplay.load() * beatsTable[jlimit (0, 5, roundToInt (v[space::beats]))] : v[space::time];
        caption (g, r, v[space::freeze] > 0.5 ? "FROZEN SPACE" : (v[space::warp] > 0.5 ? "TIME TUNNEL  WARP" : "TIME TUNNEL"), (T >= 10 ? String (T, 1) : String (T, 2)) + " s");
        auto plot = plotArea();
        grid (g, plot, 12, 4);
        // tap swarm: 64 lines whose heights follow the envelope
        const double sh = v[space::shape], ct = v[space::contour];
        Path env;
        for (int i = 0; i < 64; ++i)
        {
            const double p = (i + 0.5) / 64.0, e = SpaceProcessor::envelopeAt (p, sh, ct);
            const float x = plot.getX() + plot.getWidth() * float (p), y = plot.getBottom() - plot.getHeight() * float (e) * 0.92f;
            g.setColour (theme.accent.withAlpha (0.10f + 0.25f * float (v[space::texture])));
            g.drawLine (x, plot.getBottom(), x, y, 2.0f * scale);
        }
        for (int i = 0; i <= 160; ++i)
        {
            const double p = i / 160.0, e = SpaceProcessor::envelopeAt (p, sh, ct);
            const float x = plot.getX() + plot.getWidth() * float (p), y = plot.getBottom() - plot.getHeight() * float (e) * 0.92f;
            if (i == 0) env.startNewSubPath (x, y); else env.lineTo (x, y);
        }
        // colour over time: tint the curve from cool to warm as Color rises
        draw::glowStroke (g, env, theme.accent.interpolatedWith (v[space::colour] >= 0 ? Colour (0xfffff1c2) : Colour (0xff5b7cff), float (std::abs (v[space::colour])) * 0.6f), 2.2f * scale, 1.2f);
        // shape handle: x = Shape, y = Contour (top = flat/gated, bottom = fully shaped)
        const auto h = handlePos (v);
        g.setColour (theme.accent2.withAlpha (0.25f)); g.fillEllipse (h.x - 12 * scale, h.y - 12 * scale, 24 * scale, 24 * scale);
        g.setColour (Colours::white); g.drawEllipse (h.x - 7 * scale, h.y - 7 * scale, 14 * scale, 14 * scale, 1.6f * scale);
        g.setFont (font (9.5f * scale, true, 0.15f)); g.setColour (theme.inkDim);
        g.drawText ("DECAY", plot.withHeight (12 * scale).translated (0, plot.getHeight() + 4 * scale), Justification::left);
        g.drawText ("BLOOM", plot.withHeight (12 * scale).translated (0, plot.getHeight() + 4 * scale), Justification::centred);
        g.drawText ("REVERSE", plot.withHeight (12 * scale).translated (0, plot.getHeight() + 4 * scale), Justification::right);
    }
    void mouseDown (const MouseEvent& e) override { dragging = handlePos (proc.values()).getDistanceFrom (e.position) < 22 * scale || plotArea().contains (e.position); if (dragging) { begin(); mouseDrag (e); } }
    void mouseDrag (const MouseEvent& e) override
    {
        if (! dragging) return;
        auto plot = plotArea();
        set (space::shape, jlimit (0.0f, 1.0f, (e.position.x - plot.getX()) / plot.getWidth()));
        set (space::contour, jlimit (0.0f, 1.0f, (e.position.y - plot.getY()) / plot.getHeight()));
    }
    void mouseUp (const MouseEvent&) override { if (dragging) { end(); dragging = false; } }
private:
    Rectangle<float> plotArea() const { return screen().reduced (18 * scale, 30 * scale).withTrimmedBottom (12 * scale); }
    Point<float> handlePos (const Values& v) const { auto p = plotArea(); return { p.getX() + p.getWidth() * float (v[space::shape]), p.getY() + p.getHeight() * float (v[space::contour]) }; }
    void set (int slot, float value) { if (auto* p = proc.parameter (slot)) p->setValueNotifyingHost (p->convertTo0to1 (value)); }
    void begin() { for (int s : { int (space::shape), int (space::contour) }) if (auto* p = proc.parameter (s)) p->beginChangeGesture(); }
    void end() { for (int s : { int (space::shape), int (space::contour) }) if (auto* p = proc.parameter (s)) p->endChangeGesture(); }
    bool dragging = false;
};

// ------------------------------------------------------------------ TONE: EQ curve
class ToneDisplay : public Display
{
public:
    using Display::Display;
    void paint (Graphics& g) override
    {
        const auto v = proc.values();
        auto r = screen();
        static const char* mids[] = { "700 Hz", "1 kHz", "1.5 kHz", "2.3 kHz", "3.5 kHz", "5.6 kHz" };
        caption (g, r, String ("MID  ") + mids[jlimit (0, 5, roundToInt (v[tone::midfreq]))], v[tone::colour] > 0.5 ? "AMP  " + String (v[tone::drive] > 0 ? "+" : "") + String (v[tone::drive], 1) + " dB" : "AMP OFF");
        auto plot = r.reduced (18 * scale, 30 * scale);
        if (v != last) { last = v; compute (v); }
        // frequency grid
        g.setFont (font (9.0f * scale, true));
        for (double f : { 30.0, 100.0, 300.0, 1000.0, 3000.0, 10000.0 })
        {
            const float x = plot.getX() + plot.getWidth() * float (std::log (f / 20.0) / std::log (1000.0));
            g.setColour (theme.accent.withAlpha (0.07f)); g.drawVerticalLine (roundToInt (x), plot.getY(), plot.getBottom());
            g.setColour (theme.inkFaint); g.drawText (f >= 1000 ? String (int (f / 1000)) + "k" : String (int (f)), Rectangle<float> (x - 16 * scale, plot.getBottom() + 2 * scale, 32 * scale, 12 * scale), Justification::centred);
        }
        for (int db : { -12, -6, 0, 6, 12 })
        {
            const float y = plot.getCentreY() - plot.getHeight() * 0.5f * float (db) / 16.0f;
            g.setColour (theme.accent.withAlpha (db == 0 ? 0.18f : 0.06f)); g.drawHorizontalLine (roundToInt (y), plot.getX(), plot.getRight());
        }
        Path p, fill;
        for (size_t i = 0; i < resp.size(); ++i)
        {
            const float x = plot.getX() + plot.getWidth() * float (i) / float (resp.size() - 1), y = plot.getCentreY() - plot.getHeight() * 0.5f * jlimit (-16.0f, 16.0f, resp[i]) / 16.0f;
            if (i == 0) { p.startNewSubPath (x, y); fill.startNewSubPath (x, plot.getCentreY()); }
            p.lineTo (x, y); fill.lineTo (x, y);
        }
        fill.lineTo (plot.getRight(), plot.getCentreY()); fill.closeSubPath();
        g.setGradientFill (ColourGradient (theme.accent.withAlpha (0.25f), 0, plot.getY(), theme.accent.withAlpha (0.02f), 0, plot.getCentreY(), false));
        g.fillPath (fill);
        draw::glowStroke (g, p, theme.accent, 2.2f * scale, 1.2f);
    }
private:
    void compute (const Values& v)
    {
        // run the actual EQ section on an impulse and read its spectrum at log-spaced frequencies
        const double fs = 48000; Values w = v; w[tone::colour] = 0; w[mixSlot] = 1; w[outSlot] = 0;
        // two identical processors: impulse minus silence cancels the circuit noise floor
        ColourProcessor cp, ref; cp.prepare (Effect::Tone, fs); ref.prepare (Effect::Tone, fs);
        std::vector<double> h (8192);
        for (size_t i = 0; i < h.size(); ++i) h[i] = cp.tick ({ i == 0 ? 1.0 : 0.0, 0 }, w).l - ref.tick ({ 0.0, 0 }, w).l;
        resp.resize (180);
        for (size_t k = 0; k < resp.size(); ++k)
        {
            const double f = 20.0 * std::pow (1000.0, double (k) / double (resp.size() - 1));
            const std::complex<double> step = std::polar (1.0, -2 * pi * f / fs);
            std::complex<double> acc = 0, ph = 1;
            for (size_t n = 0; n < h.size(); ++n) { acc += h[n] * ph; ph *= step; }
            resp[k] = float (20 * std::log10 (std::abs (acc) + 1e-9));
        }
    }
    Values last {}; std::vector<float> resp = std::vector<float> (180, 0.0f);
};

// ------------------------------------------------------------------ VOICE: detected note -> output note
class VoiceDisplay : public Display
{
public:
    using Display::Display;
    void tick() override
    {
        const float hz = proc.voiceHz.load();
        if (hz > 40) inHz = inHz > 0 ? 0.7f * inHz + 0.3f * hz : hz;
        shift = 0.7f * shift + 0.3f * proc.voiceShift.load();
        repaint();
    }
    void paint (Graphics& g) override
    {
        const auto v = proc.values();
        auto r = screen();
        static const char* modes[] = { "TRANSPOSE", "QUANTIZE", "ROBOT" };
        caption (g, r, modes[jlimit (0, 2, roundToInt (v[voice::mode]))], v[voice::link] > 0.5 ? "FORMANT LINKED" : "FORMANT " + String (v[voice::formant], 1) + " st");
        auto plot = r.reduced (18 * scale, 30 * scale);
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        auto noteOf = [] (double hz) { return 69.0 + 12.0 * std::log2 (hz / 440.0); };
        const double inNote = inHz > 0 ? noteOf (inHz) : -1, outNote = inNote >= 0 ? inNote + shift : -1;
        auto label = [&] (double n) { if (n < 0) return String ("--"); const int k = roundToInt (n); return String (names[(k % 12 + 12) % 12]) + String (k / 12 - 1); };
        auto centsOf = [] (double n) { return roundToInt ((n - std::round (n)) * 100); };
        auto top = plot.removeFromTop (plot.getHeight() * 0.58f);
        auto left = top.removeFromLeft (top.getWidth() * 0.42f), right = top.withTrimmedLeft (top.getWidth() * 0.16f);
        g.setFont (font (11.0f * scale, true, 0.2f)); g.setColour (theme.inkDim);
        g.drawText ("IN", left, Justification::topLeft); g.drawText ("OUT", right, Justification::topLeft);
        g.setFont (font (44.0f * scale, true)); g.setColour (theme.ink.withAlpha (0.6f)); g.drawText (label (inNote), left, Justification::centredLeft);
        g.setColour (theme.accent); g.drawText (label (outNote), right, Justification::centredLeft);
        g.setFont (font (12.0f * scale, true)); g.setColour (theme.inkDim);
        if (inNote >= 0) g.drawText ((centsOf (outNote) >= 0 ? "+" : "") + String (centsOf (outNote)) + " ct", right, Justification::bottomLeft);
        Path arrow; const auto ac = Point<float> (top.getX() + top.getWidth() * 0.0f + left.getWidth() + 4 * scale, left.getCentreY());
        arrow.addArrow (Line<float> (left.getRight() - 20 * scale, left.getCentreY(), right.getX() - 14 * scale, left.getCentreY()), 2.0f * scale, 10 * scale, 10 * scale);
        g.setColour (theme.accent.withAlpha (0.7f)); g.fillPath (arrow); (void) ac;
        // two-octave keyboard
        auto kb = plot.reduced (0, 6 * scale);
        const int first = 48, keys = 36;
        int whites = 0; for (int k = first; k < first + keys; ++k) { const int pc = k % 12; whites += (pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10) ? 0 : 1; }
        const float kw = kb.getWidth() / float (whites);
        auto fold = [&] (double n) { if (n < 0) return -1; int k = roundToInt (n); while (k < first) k += 12; while (k >= first + keys) k -= 12; return k; };
        const int inK = fold (inNote), outK = fold (outNote);
        int wi = 0;
        for (int k = first; k < first + keys; ++k)
        {
            const int pc = k % 12; if (pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10) continue;
            auto key = Rectangle<float> (kb.getX() + kw * float (wi++), kb.getY(), kw - 1.5f * scale, kb.getHeight());
            g.setColour (k == outK ? theme.accent : k == inK ? theme.ink.withAlpha (0.45f) : Colour (0xffcfcac2).withAlpha (0.16f));
            g.fillRoundedRectangle (key, 2 * scale);
        }
        wi = 0;
        for (int k = first; k < first + keys; ++k)
        {
            const int pc = k % 12; const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
            if (! black) { ++wi; continue; }
            auto key = Rectangle<float> (kb.getX() + kw * float (wi) - kw * 0.32f, kb.getY(), kw * 0.64f, kb.getHeight() * 0.6f);
            g.setColour (k == outK ? theme.accent : k == inK ? theme.ink.withAlpha (0.55f) : Colour (0xff0b0a0b));
            g.fillRoundedRectangle (key, 2 * scale);
        }
    }
private:
    float inHz = 0, shift = 0;
};
}

std::unique_ptr<Display> makeDisplay (ExpansionProcessor& p, const Theme& t)
{
    switch (p.effect)
    {
        case Effect::Heat:  return std::make_unique<HeatDisplay> (p, t);
        case Effect::Valve: return std::make_unique<MeterDisplay> (p, t, false);
        case Effect::Crush: return std::make_unique<MeterDisplay> (p, t, true);
        case Effect::Prism: return std::make_unique<PrismDisplay> (p, t);
        case Effect::Time:  return std::make_unique<TimeDisplay> (p, t);
        case Effect::Plate: return std::make_unique<PlateDisplay> (p, t);
        case Effect::Space: return std::make_unique<SpaceDisplay> (p, t);
        case Effect::Tone:  return std::make_unique<ToneDisplay> (p, t);
        case Effect::Voice: return std::make_unique<VoiceDisplay> (p, t);
    }
    return nullptr;
}

// ================================================================== spectrum strip
Spectrum::Spectrum (ExpansionProcessor& p, const Theme& t) : proc (p), theme (t)
{
    window.resize (size_t (size)); work.resize (size_t (2 * size));
    for (int i = 0; i < size; ++i) window[size_t (i)] = 0.5f - 0.5f * std::cos (2.0f * MathConstants<float>::pi * float (i) / float (size - 1));
    inDb.assign (size_t (points), -100.0f); outDb.assign (size_t (points), -100.0f);
    setInterceptsMouseClicks (false, false);
}
static float spectrumFreq (int i, int n) { return 25.0f * std::pow (20000.0f / 25.0f, float (i) / float (n - 1)); }
void Spectrum::analyse (const std::array<std::atomic<float>, ExpansionProcessor::analyserSize>& src, std::vector<float>& smoothed, unsigned w)
{
    std::fill (work.begin(), work.end(), 0.0f);
    for (int i = 0; i < size; ++i) work[size_t (i)] = src[(w - unsigned (size) + unsigned (i)) & (ExpansionProcessor::analyserSize - 1)].load (std::memory_order_relaxed) * window[size_t (i)];
    fft.performFrequencyOnlyForwardTransform (work.data(), true);
    const float fs = float (proc.analyserRate()), binHz = fs / float (size);
    const float norm = 4.0f / float (size);                         // Hann-windowed sine -> 0 dBFS peak
    for (int i = 0; i < points; ++i)
    {
        // band-average the bins that fall between neighbouring display points
        const float f0 = spectrumFreq (i, points) / std::pow (20000.0f / 25.0f, 0.5f / float (points - 1));
        const float f1 = f0 * std::pow (20000.0f / 25.0f, 1.0f / float (points - 1));
        int b0 = jlimit (1, size / 2 - 1, int (f0 / binHz)), b1 = jlimit (b0, size / 2 - 1, int (f1 / binHz));
        float m = 0; for (int b = b0; b <= b1; ++b) m = jmax (m, work[size_t (b)]);
        // +3 dB/oct tilt so music reads flat, like a pink-noise referenced analyser
        float db = 20.0f * std::log10 (m * norm + 1e-7f) + 3.0f * std::log2 (spectrumFreq (i, points) / 1000.0f);
        auto& s = smoothed[size_t (i)];
        s = db > s ? db : s + (db - s) * 0.18f;
    }
}
void Spectrum::tick()
{
    const unsigned w = proc.analyserWrite.load (std::memory_order_relaxed);
    if (w == lastWrite && settled) return;                    // no new audio and the curves have come to rest
    lastWrite = w;
    const auto before = outDb, beforeIn = inDb;
    analyse (proc.analyserIn, inDb, w);
    analyse (proc.analyserOut, outDb, w);
    float moved = 0;
    for (size_t i = 0; i < outDb.size(); ++i) moved = jmax (moved, std::abs (outDb[i] - before[i]), std::abs (inDb[i] - beforeIn[i]));
    settled = moved < 0.05f;
    if (moved > 0.25f || ! drawnOnce) { drawnOnce = true; repaint(); }
}
void Spectrum::resized() { staticScale = 0.0f; }
// Grid, frequency labels and legend never change: drawn once into a bitmap at the screen's pixel density.
void Spectrum::renderStatic (float px)
{
    staticScale = px;
    staticLayer = Image (Image::ARGB, jmax (1, roundToInt ((float) getWidth() * px)), jmax (1, roundToInt ((float) getHeight() * px)), true);
    Graphics g (staticLayer);
    g.addTransform (AffineTransform::scale (px));
    auto r = getLocalBounds().toFloat();
    auto plot = r.reduced (12 * scale, 0).withTrimmedTop (24 * scale).withTrimmedBottom (18 * scale);
    g.setFont (font (8.5f * scale, true, 0.1f));
    for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const float x = plot.getX() + plot.getWidth() * std::log (f / 25.0f) / std::log (20000.0f / 25.0f);
        g.setColour (theme.accent.withAlpha (0.06f)); g.drawVerticalLine (roundToInt (x), plot.getY(), plot.getBottom());
        g.setColour (theme.inkFaint);
        g.drawText (f >= 1000 ? String (int (f / 1000)) + "k" : String (int (f)), Rectangle<float> (x - 20 * scale, plot.getBottom() + 3 * scale, 40 * scale, 12 * scale), Justification::centred);
    }
    for (int d = 1; d < 4; ++d) { g.setColour (theme.accent.withAlpha (0.05f)); g.drawHorizontalLine (roundToInt (plot.getY() + plot.getHeight() * float (d) / 4.0f), plot.getX(), plot.getRight()); }
    g.setFont (font (9.0f * scale, true, 0.22f));
    auto head = r.reduced (12 * scale, 6 * scale).withHeight (14 * scale);
    g.setColour (theme.inkDim); g.drawText ("SPECTRUM", head, Justification::left);
    auto leg = head.removeFromRight (110 * scale);
    g.setColour (theme.ink.withAlpha (0.4f)); g.fillRect (leg.getX(), leg.getCentreY(), 12 * scale, 1.2f * scale);
    g.setColour (theme.inkDim); g.drawText ("IN", leg.withTrimmedLeft (16 * scale), Justification::left);
    g.setColour (theme.accent); g.fillRect (leg.getX() + 48 * scale, leg.getCentreY() - 0.5f * scale, 12 * scale, 2.0f * scale);
    g.setColour (theme.inkDim); g.drawText ("OUT", leg.withTrimmedLeft (64 * scale), Justification::left);
}
void Spectrum::paint (Graphics& g)
{
    const float px = jmax (1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    if (! staticLayer.isValid() || std::abs (px - staticScale) > 0.01f) renderStatic (px);
    g.setImageResamplingQuality (Graphics::lowResamplingQuality);
    g.drawImage (staticLayer, getLocalBounds().toFloat());
    auto r = getLocalBounds().toFloat();
    auto plot = r.reduced (12 * scale, 0).withTrimmedTop (24 * scale).withTrimmedBottom (18 * scale);
    const float top = -6.0f, bottom = -84.0f;
    auto curve = [&] (const std::vector<float>& db, bool close)
    {
        Path p;
        for (int i = 0; i < points; ++i)
        {
            const float x = plot.getX() + plot.getWidth() * float (i) / float (points - 1);
            const float y = jmap (jlimit (bottom, top, db[size_t (i)]), bottom, top, plot.getBottom(), plot.getY());
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        if (close) { p.lineTo (plot.getRight(), plot.getBottom()); p.lineTo (plot.getX(), plot.getBottom()); p.closeSubPath(); }
        return p;
    };
    auto fill = curve (outDb, true);
    g.setGradientFill (ColourGradient (theme.accent.withAlpha (0.38f), 0, plot.getY(), theme.accent.withAlpha (0.02f), 0, plot.getBottom(), false));
    g.fillPath (fill);
    auto inLine = curve (inDb, false);
    g.setColour (theme.ink.withAlpha (0.28f));
    g.strokePath (inLine, PathStrokeType (1.0f * scale));
    auto outLine = curve (outDb, false);
    g.setColour (theme.accent.brighter (0.3f)); g.strokePath (outLine, PathStrokeType (1.6f * scale));
}
}
