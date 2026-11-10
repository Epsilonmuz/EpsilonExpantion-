#include "PluginEditor.h"
#include "DSP/Presets.h"

using namespace juce;
using namespace eps;

namespace
{
constexpr float baseW = 920.0f, baseH = 590.0f;

String tipFor (Effect e, int slot)
{
    switch (e)
    {
        case Effect::Heat:
            switch (slot) { case heat::drive: return "Drive: how hard the signal hits the saturation stage. More drive, more harmonics.";
                case heat::tone: return "Tone: dark to bright tilt before the saturation, so it also changes which frequencies distort.";
                case heat::lowcut: return "Low Cut: removes lows before the saturation. Fully left is off.";
                case heat::highcut: return "High Cut: removes fizz after the saturation. Fully right is off.";
                case heat::style: return "Style: Tape (smooth), Console (big lows, airy top), Germanium (focused mids), Triode (even harmonics), Pentode (odd, edgy).";
                case heat::quality: return "Oversampling quality. Ultra has the cleanest highs at the highest CPU cost.";
                case heat::punish: return "Punish: adds 20 dB of drive for heavy distortion.";
                case heat::autogain: return "Auto: turns the output down as Drive goes up, so you hear tone, not loudness.";
                case heat::thump: return "Thump: a resonant bump at the Low Cut frequency, like a tape head bump.";
                case heat::steep: return "Steep: High Cut slope 30 dB/oct instead of 6 dB/oct (speaker-cabinet style).";
                default: break; } break;
        case Effect::Valve:
            switch (slot) { case valve::input: return "Input: drive into the first tube stage.";
                case valve::drive: return "Drive: level into the second tube stage, after the tone controls.";
                case valve::bass: return "Bass: +-10 dB. Cuts are wide, boosts more sloped.";
                case valve::treble: return "Treble: +-10 dB.";
                case valve::mic: return "Source: Line (flat) or Mic (lighter lows, presence lift).";
                case valve::noise: return "Noise: Noisy keeps a little circuit hiss and hum; Clean removes it.";
                case valve::autogain: return "Auto Gain: keeps the level steady as you drive the stages.";
                default: break; } break;
        case Effect::Crush:
            switch (slot) { case crush::crush: return "Crush: how hard the signal is pushed into the limiter. High settings lock everything to one level.";
                case crush::crunch: return "Crunch: drives the output amplifier into distortion.";
                case crush::darkness: return "Darkness: a high cut after the distortion.";
                case crush::release: return "Release: Slow or Fast recovery. Driving hard makes it recover much more slowly.";
                default: break; } break;
        case Effect::Prism:
            switch (slot) { case prism::pitch: return "Pitch: shift of each splice, +-2 octaves.";
                case prism::splice: return "Splice: length of each captured slice. Keep it above 30 ms for clear pitch.";
                case prism::delay: return "Delay: extra time before the splices play.";
                case prism::recycle: return "Recycle: feeds the effect back into itself for spiralling repeats.";
                case prism::smoothing: return "Smoothing: cross-fade between splices. Low is choppy, high is washy.";
                case prism::reverse: return "Direction: play each splice forwards or backwards.";
                case prism::fbmode: return "Feedback: Mixed, Dual (independent sides) or Ping-Pong.";
                case prism::sync: return "Sync: splice length follows the host tempo.";
                case prism::note: return "Splice length in note values when Sync is on.";
                case prism::gateduck: return "Gate / Duck: right ducks the effect while you play; left gates it until you play.";
                case prism::threshold: return "Threshold for Gate / Duck.";
                case prism::spread: return "Spread: detunes the left side down and the right side up.";
                case prism::offset: return "Splice Offset: shortens the right-side splices for stereo motion.";
                case prism::lowcut: return "Low Cut on the effect."; case prism::highcut: return "High Cut on the effect.";
                default: break; } break;
        case Effect::Time:
            switch (slot) { case time::timeA: return "Time A: delay of line A (left).";
                case time::timeB: return "Time B: delay of line B (right).";
                case time::feedbackA: return "Feedback A. Above 100 % it sings and saturates.";
                case time::feedbackB: return "Feedback B.";
                case time::depth: return "Depth: delay-time modulation (chorus, flange, warble).";
                case time::rate: return "Rate: modulation speed, up to audio rate.";
                case time::multiply: return "Multiply: lengthens the delay by 2, 4 or 8 and lowers the bandwidth by the same factor.";
                case time::mode: return "Mode: Classic (two lines), Ping-Pong, Series or Cross feedback.";
                case time::freeze: return "Freeze: loops what is in the delay memory.";
                case time::sync: return "Sync: times follow the host tempo.";
                case time::link: return "Link B: line B follows line A at 1.5x (dotted).";
                default: break; } break;
        case Effect::Plate:
            switch (slot) { case plate::decay: return "Decay: RT60 at 500 Hz.";
                case plate::predelay: return "Pre-Delay: gap before the reverb starts.";
                case plate::damping: return "Damping: how fast the highs die away inside the plate.";
                case plate::mod: return "Mod: gentle movement that keeps long tails lush.";
                case plate::autodecay: return "Auto-Decay: loud input shortens the tail; it grows back when you stop.";
                case plate::model: return "Plate: Steel (big, floaty), Gold Foil (compact), Bright (upper-mid edge), Warm (solid, darker), Splash (bright, splashy).";
                case plate::amp: return "Amp: Clean, Tube or Transistor colour on the way into the plate.";
                case plate::freeze: return "Freeze: sustains the current tail indefinitely.";
                default: break; } break;
        case Effect::Space:
            switch (slot) { case space::time: return "Time: length of the space, 0.1 s to 60 s.";
                case space::shape: return "Shape: decay, bloom (fade in and out) or reverse. You can also drag the point in the display.";
                case space::contour: return "Contour: top of the display is a flat gate; lower is more strongly shaped.";
                case space::colour: return "Color: darkens or brightens progressively over time.";
                case space::texture: return "Texture: discrete echoes at low settings, smooth and diffuse at high settings.";
                case space::mod: return "Mod: chorus-like movement inside the space.";
                case space::freeze: return "Freeze: captures the space and keeps it going.";
                case space::warp: return "Warp: changing Time glides like tape (pitch moves) instead of jumping.";
                case space::sync: return "Sync: Time follows the host tempo."; case space::beats: return "Length in beats when Sync is on.";
                default: break; } break;
        case Effect::Tone:
            switch (slot) { case tone::low: return "Low: cut is a shelf, boost is a broad bell at the very bottom.";
                case tone::mid: return "Mid: proportional-Q bell at the selected frequency.";
                case tone::high: return "High: cut is a gentle roll-off, boost is a broad bell at the very top.";
                case tone::drive: return "Drive: into the amplifier stage, with automatic level compensation.";
                case tone::midfreq: return "Mid frequency.";
                case tone::colour: return "Amp: the output amplifier and its colour. Off is a clean EQ.";
                default: break; } break;
        case Effect::Voice:
            switch (slot) { case voice::pitch: return "Pitch: +-12 semitones. In Robot mode it picks the note the voice is held on (0 = C).";
                case voice::formant: return "Formant: deeper (left) or brighter (right) vocal character.";
                case voice::drive: return "Drive: tube saturation after the shift.";
                case voice::tone: return "Tone: high cut on the processed voice.";
                case voice::mode: return "Mode: Transpose, Quantize (snap to the nearest semitone) or Robot (hold one note).";
                case voice::range: return "Voice: helps formant tracking for low, mid or high voices.";
                case voice::link: return "Link: formants follow the pitch, like a sample sped up or slowed down.";
                default: break; } break;
    }
    if (slot == mixSlot) return "Mix: dry/wet balance.";
    if (slot == outSlot) return "Output level.";
    if (slot == tempoSlot) return "Tempo used when the host does not provide one.";
    return {};
}
}

ExpansionEditor::Layout ExpansionEditor::layoutFor (Effect e)
{
    Layout l;
    switch (e)
    {
        case Effect::Heat:  l = { heat::drive, { heat::tone, heat::lowcut, heat::highcut }, { heat::style }, { heat::punish, heat::thump, heat::steep, heat::autogain }, { heat::quality, mixSlot, outSlot }, {} }; break;
        case Effect::Valve: l = { valve::input, { valve::drive, valve::bass, valve::treble }, { valve::mic, valve::noise }, { valve::autogain }, { mixSlot, outSlot }, {} }; break;
        case Effect::Crush: l = { crush::crush, { crush::crunch, crush::darkness }, { crush::release }, {}, { mixSlot, outSlot }, {} }; break;
        case Effect::Prism: l = { prism::pitch, { prism::splice, prism::delay, prism::recycle, prism::smoothing }, { prism::reverse, prism::fbmode }, { prism::sync },
                                  { prism::note, mixSlot, outSlot }, { prism::gateduck, prism::threshold, prism::spread, prism::offset, prism::lowcut, prism::highcut, tempoSlot } }; break;
        case Effect::Time:  l = { time::timeA, { time::timeB, time::feedbackA, time::feedbackB, time::depth }, { time::multiply, time::mode }, { time::freeze, time::sync, time::link },
                                  { time::rate, mixSlot, outSlot }, { time::levelA, time::levelB, time::lowcut, time::highcut, tempoSlot, time::shape, time::noteA, time::noteB } }; break;
        case Effect::Plate: l = { plate::decay, { plate::predelay, plate::damping, plate::mod, plate::autodecay }, { plate::model }, { plate::freeze },
                                  { plate::amp, mixSlot, outSlot }, { plate::lowcut, plate::highcut, plate::width, plate::rate } }; break;
        case Effect::Space: l = { space::time, { space::texture, space::colour, space::mod, space::shape }, { space::beats }, { space::freeze, space::sync, space::warp },
                                  { space::contour, tempoSlot, mixSlot, outSlot }, {} }; break;
        case Effect::Tone:  l = { -1, { tone::low, tone::mid, tone::high, tone::drive }, { tone::midfreq }, { tone::colour }, { mixSlot, outSlot }, {} }; break;
        case Effect::Voice: l = { voice::pitch, { voice::formant, voice::drive, voice::tone }, { voice::mode, voice::range }, { voice::link }, { mixSlot, outSlot }, {} }; break;
    }
    return l;
}

ExpansionEditor::ExpansionEditor (ExpansionProcessor& p)
    : AudioProcessorEditor (p), proc (p), theme (ui::themeFor (p.effect)), lnf (theme), layout (layoutFor (p.effect))
{
    setLookAndFeel (&lnf);
    // draw with JUCE's software renderer (much lighter than CoreGraphics on Macs); set
    // EPSILON_NATIVE_RENDER=1 in the environment to compare against the system renderer
    if (juce::SystemStats::getEnvironmentVariable ("EPSILON_NATIVE_RENDER", {}) != "1")
        setCachedComponentImage (new ui::SoftwareCachedImage (*this));
    display = ui::makeDisplay (proc, theme);
    addAndMakeVisible (*display);
    spectrum = std::make_unique<ui::Spectrum> (proc, theme);
    addAndMakeVisible (*spectrum);

    auto& st = proc.state;
    auto isChoice = [this] (int slot) { return proc.info.spec[size_t (slot)].choices != nullptr; };
    auto isToggle = [this] (int slot) { const auto* c = proc.info.spec[size_t (slot)].choices; return c && String (c) == "Off|On"; };
    auto addKnob = [&] (int slot, ui::Knob::Size sz)
    {
        const auto& sp = proc.info.spec[size_t (slot)];
        auto k = std::make_unique<ui::Knob> (st, sp.id, sp.name, sz, tipFor (proc.effect, slot), sp.unit == Unit::Bipolar || (sp.lo < 0 && sp.hi > 0 && sp.unit != Unit::Db) || (sp.unit == Unit::Db && sp.lo < 0 && slot != outSlot));
        addAndMakeVisible (*k); bySlot[slot] = k.get(); knobs.push_back (std::move (k));
    };
    auto addSegment = [&] (int slot)
    {
        auto sg = std::make_unique<ui::Segmented> (st, proc.info.spec[size_t (slot)].id, theme, tipFor (proc.effect, slot));
        sg->caption = String (proc.info.spec[size_t (slot)].name).toUpperCase();
        addAndMakeVisible (*sg); bySlot[slot] = sg.get(); segments.push_back (std::move (sg));
    };
    auto addSwitch = [&] (int slot)
    {
        auto sw = std::make_unique<ui::LedSwitch> (st, proc.info.spec[size_t (slot)].id, proc.info.spec[size_t (slot)].name, theme, tipFor (proc.effect, slot));
        addAndMakeVisible (*sw); bySlot[slot] = sw.get(); switches.push_back (std::move (sw));
    };
    auto addAny = [&] (int slot, ui::Knob::Size sz) { if (isToggle (slot)) addSwitch (slot); else if (isChoice (slot)) addSegment (slot); else addKnob (slot, sz); };

    if (layout.hero >= 0) addKnob (layout.hero, ui::Knob::Hero);
    for (int s2 : layout.secondary) addKnob (s2, ui::Knob::Large);
    for (int s2 : layout.strip) addSegment (s2);
    for (int s2 : layout.toggles) addSwitch (s2);
    for (int s2 : layout.bottom) addAny (s2, s2 == mixSlot || s2 == outSlot ? ui::Knob::Medium : ui::Knob::Small);
    for (int s2 : layout.tweak) addAny (s2, ui::Knob::Small);
    addAndMakeVisible (inMeter); addAndMakeVisible (outMeter);

    // header
    prev.glyphArrowLeft = true; next.glyphArrowRight = true; bypass.glyphPower = true; inspire.glyphDice = true;
    bankA.setClickingTogglesState (false); bankB.setClickingTogglesState (false);
    for (auto* b : { &prev, &next, &presetName, &bankA, &bankB, &inspire, &bypass }) addAndMakeVisible (*b);
    if (! layout.tweak.empty()) addAndMakeVisible (tweak);
    prev.onClick = [this] { loadPreset ((proc.getCurrentProgram() + presetCount - 1) % presetCount); };
    next.onClick = [this] { loadPreset ((proc.getCurrentProgram() + 1) % presetCount); };
    presetName.onClick = [this] { buildPresetMenu(); };
    inspire.onClick = [this] { auto r = Random::getSystemRandom(); int i; do i = 1 + r.nextInt (presetCount - 1); while (i == proc.getCurrentProgram()); loadPreset (i); };
    bankA.onClick = [this] { switchBank (0); };
    bankB.onClick = [this] { switchBank (1); };
    tweak.onClick = [this] { setTweakVisible (! tweakVisible); };
    prev.setTooltip ("Previous preset"); next.setTooltip ("Next preset"); presetName.setTooltip ("Factory presets");
    inspire.setTooltip ("Inspire: load a random factory preset"); bankA.setTooltip ("Compare: setting A"); bankB.setTooltip ("Compare: setting B");
    tweak.setTooltip ("Tweak: advanced controls"); bypass.setTooltip ("Bypass (keeps the plugin's latency)");
    if (auto* bp = proc.parameter (bypassSlot))
    {
        bypassAttachment = std::make_unique<ParameterAttachment> (*bp, [this] (float v) { bypass.setToggleState (v > 0.5f, dontSendNotification); });
        bypassAttachment->sendInitialUpdate();
        bypass.onClick = [this] { bypassAttachment->setValueAsCompleteGesture (bypass.getToggleState() ? 0.0f : 1.0f); };
    }
    banks[0] = proc.values(); bankUsed[0] = true;
    setTweakVisible (false);

    setResizable (true, true);
    setResizeLimits (int (baseW * 0.7f), int (baseH * 0.7f), int (baseW * 2.0f), int (baseH * 2.0f));
    if (auto* c = getConstrainer()) c->setFixedAspectRatio (baseW / baseH);
    setSize (int (baseW), int (baseH));
    startTimerHz (60);
}

ExpansionEditor::~ExpansionEditor() { stopTimer(); setLookAndFeel (nullptr); }

void ExpansionEditor::setTweakVisible (bool v)
{
    tweakVisible = v;
    tweak.setToggleState (v, dontSendNotification);
    for (int slot : layout.tweak) if (auto* c = controlFor (slot)) c->setVisible (v);
    display->setVisible (! v);
    for (int slot : layout.strip) if (auto* c = controlFor (slot)) c->setVisible (! v);
    repaint();
}
Component* ExpansionEditor::controlFor (int slot) { auto it = bySlot.find (slot); return it == bySlot.end() ? nullptr : it->second; }

void ExpansionEditor::buildPresetMenu()
{
    PopupMenu m;
    m.addSectionHeader ("FACTORY PRESETS");
    for (int i = 0; i < presetCount; ++i) m.addItem (i + 1, String (i + 1).paddedLeft ('0', 2) + "   " + eps::presetName (proc.effect, i), true, i == proc.getCurrentProgram());
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&presetName).withMinimumWidth (presetName.getWidth()),
                     [this] (int r) { if (r > 0) loadPreset (r - 1); });
}
void ExpansionEditor::loadPreset (int index) { proc.setCurrentProgram (index); repaint(); }
void ExpansionEditor::switchBank (int b)
{
    if (b == bank) return;
    banks[size_t (bank)] = proc.values(); bankUsed[size_t (bank)] = true;
    if (! bankUsed[size_t (b)]) { banks[size_t (b)] = banks[size_t (bank)]; bankUsed[size_t (b)] = true; }
    bank = b;
    auto v = banks[size_t (b)]; v[bypassSlot] = proc.values()[bypassSlot];
    proc.applyValues (v);
}

void ExpansionEditor::timerCallback()
{
    // Peaks are collected every tick so short hits are never missed.
    pendingIn = jmax (pendingIn, proc.inputPeak.exchange (0.0f));
    pendingOut = jmax (pendingOut, proc.outputPeak.exchange (0.0f));
    const bool even = (frame++ & 1u) == 0;
    if (even)
    {
        // Is anything happening? Signal in or out (reverb tails count), or a control moved.
        const auto v = proc.values();
        const bool moved = v != lastValues;
        lastValues = v;
        quietTicks = jmax (pendingIn, pendingOut) > 1.0e-4f ? 0 : jmin (quietTicks + 1, 1000);
        const bool live = quietTicks < 30;                       // keep animating for 1 s after the sound stops
        if (display->isVisible() && (live || moved)) display->tick();
        presetName.setButtonText (String (proc.getCurrentProgram() + 1).paddedLeft ('0', 2) + "   " + proc.getProgramName (proc.getCurrentProgram()));
        bankA.setToggleState (bank == 0, dontSendNotification);
        bankB.setToggleState (bank == 1, dontSendNotification);
    }
    else
    {
        if (spectrum->isShowing() || ! isShowing()) spectrum->tick();     // repaints only when a curve moved
        inMeter.push (pendingIn);                                         // repaint only when a segment changes
        outMeter.push (pendingOut);
        pendingIn = pendingOut = 0;
    }
}
void ExpansionEditor::refreshForSnapshot (int frames) { for (int i = 0; i < 2 * frames; ++i) timerCallback(); }

Rectangle<int> ExpansionEditor::sc (float x, float y, float w, float h) const { return Rectangle<float> (x * s, y * s, w * s, h * s).toNearestInt(); }

void ExpansionEditor::resized()
{
    s = float (getWidth()) / baseW;
    lnf.scale = s;
    display->scale = s;
    spectrum->scale = s;
    for (auto& k : knobs) k->scale = s;
    for (auto& k : switches) k->scale = s;
    for (auto& k : segments) k->scale = s;
    inMeter.scale = outMeter.scale = s;
    for (auto* b : { &prev, &next, &presetName, &bankA, &bankB, &inspire, &tweak, &bypass }) b->scale = s;

    // header
    prev.setBounds (sc (330, 20, 30, 30)); presetName.setBounds (sc (364, 20, 228, 30)); next.setBounds (sc (596, 20, 30, 30));
    bankA.setBounds (sc (644, 20, 32, 30)); bankB.setBounds (sc (678, 20, 32, 30));
    inspire.setBounds (sc (722, 20, 92, 30));
    tweak.setBounds (sc (818, 20, 50, 30));
    if (layout.tweak.empty()) inspire.setBounds (sc (746, 20, 110, 30));
    bypass.setBounds (sc (874, 20, 30, 30));

    // display + strip
    display->setBounds (sc (28, 88, 444, 240));
    // strip selectors share the width in proportion to how much text they carry
    {
        std::vector<float> weight;
        float total = 0;
        for (int slot : layout.strip)
        {
            StringArray opts; opts.addTokens (proc.info.spec[size_t (slot)].choices, "|", "");
            float w = 0; for (auto& o : opts) w += 2.0f + float (o.length());
            weight.push_back (w); total += w;
        }
        float x0 = 28;
        const float avail = 444.0f - 12.0f * float (layout.strip.size() - 1);
        for (size_t i = 0; i < layout.strip.size(); ++i)
        {
            const float w = avail * weight[i] / total;
            if (auto* c = controlFor (layout.strip[i])) c->setBounds (sc (x0, 346, w, 32));
            x0 += w + 12;
        }
    }
    // tweak panel (same area): knobs on a 4-wide grid, captioned selectors below
    {
        int ki = 0;
        for (int slot : layout.tweak)
            if (auto* c = controlFor (slot); c && dynamic_cast<ui::Segmented*> (c) == nullptr)
            { const int col = ki % 5, row = ki / 5; c->setBounds (sc (40 + float (col) * 84, 116 + float (row) * 104, 84, 100)); ++ki; }
        float y = 116 + float ((ki + 4) / 5) * 104 + 6;
        for (int slot : layout.tweak)
            if (auto* c = controlFor (slot); c && dynamic_cast<ui::Segmented*> (c) != nullptr) { c->setBounds (sc (140, y, 316, 28)); y += 36; }
    }
    // hero + secondary
    const int nSec = int (layout.secondary.size());
    if (layout.hero >= 0)
    {
        if (auto* c = controlFor (layout.hero)) c->setBounds (sc (492, 92, 176, 226));
        for (int i = 0; i < nSec; ++i)
        {
            const int col = i % 2, row = i / 2;
            const float yOff = nSec <= 2 ? 58.0f : 0.0f;
            // an odd last knob sits centred under the pair
            const float xOff = (nSec % 2 == 1 && i == nSec - 1) ? 56.0f : 0.0f;
            if (auto* c = controlFor (layout.secondary[size_t (i)])) c->setBounds (sc (680 + float (col) * 112 + xOff, 92 + yOff + float (row) * 118, 104, 116));
        }
    }
    else
        for (int i = 0; i < nSec; ++i)
            if (auto* c = controlFor (layout.secondary[size_t (i)])) c->setBounds (sc (494 + float (i) * 102, 100, 98, 196));
    // switches
    const int nSw = int (layout.toggles.size());
    for (int i = 0; i < nSw; ++i)
    {
        const float w = nSw == 1 ? 140.0f : (402.0f - 10.0f * float (nSw - 1)) / float (nSw);
        if (auto* c = controlFor (layout.toggles[size_t (i)])) c->setBounds (sc ((nSw == 1 ? 494 + (402 - w) * 0.5f : 494) + float (i) * (w + 10), 338, w, 30));
    }
    // bottom row: small controls left, Mix/Output, meters at the far right
    float x = 44;
    for (int slot : layout.bottom)
    {
        auto* c = controlFor (slot); if (! c || slot == mixSlot || slot == outSlot) continue;
        if (dynamic_cast<ui::Segmented*> (c)) { const float w = proc.info.spec[size_t (slot)].hi >= 5 ? 280.0f : 190.0f; c->setBounds (sc (x, 470, w, 32)); x += w + 24; }
        else if (dynamic_cast<ui::LedSwitch*> (c)) { c->setBounds (sc (x, 470, 120, 30)); x += 136; }
        else { const bool big = slot == mixSlot || slot == outSlot; c->setBounds (sc (x, big ? 410 : 420, big ? 100.0f : 92.0f, big ? 140.0f : 124.0f)); x += big ? 108 : 100; }
    }
    // the mix / output pair is right-aligned before the meters
    spectrumPanel = { x > 44 ? x - 4.0f : 30.0f, 412.0f, (x > 44 ? 632.0f - x : 600.0f), 128.0f };
    spectrum->setBounds (sc (spectrumPanel.getX(), spectrumPanel.getY(), spectrumPanel.getWidth(), spectrumPanel.getHeight()));
    if (auto* mix = controlFor (mixSlot)) mix->setBounds (sc (640, 410, 100, 140));
    if (auto* out = controlFor (outSlot)) out->setBounds (sc (748, 410, 100, 140));
    inMeter.setBounds (sc (856, 412, 22, 136));
    outMeter.setBounds (sc (880, 412, 22, 136));

    backgroundScale = 0.0f;                     // re-rendered at the screen's pixel density on the next paint
}

// The faceplate is drawn once into a bitmap at the screen's real pixel density (2x on Retina),
// so each repaint is a straight copy instead of a rescale of the whole panel.
void ExpansionEditor::renderBackground (float pixelScale)
{
    backgroundScale = pixelScale;
    background = Image (Image::RGB, jmax (1, roundToInt ((float) getWidth() * pixelScale)), jmax (1, roundToInt ((float) getHeight() * pixelScale)), false, SoftwareImageType());
    Graphics g (background);
    g.addTransform (AffineTransform::scale (pixelScale));
    const auto R = getLocalBounds().toFloat();
    ui::draw::faceplate (g, R, theme, s);
    // header band
    auto head = R.withHeight (70 * s);
    g.setColour (Colours::black.withAlpha (0.35f)); g.fillRect (head);
    g.setColour (Colours::white.withAlpha (0.06f)); g.fillRect (head.withY (head.getBottom() - 1).withHeight (1.0f));
    g.setColour (Colours::black.withAlpha (0.6f)); g.fillRect (head.withY (head.getBottom()).withHeight (1.0f));
    // Epsilon Sounds product mark (shared design system); the effect's hue marks the tagline only
    {
        const float right = epsilon_ui::drawProductMark (g, { 24 * s, 20 * s }, s, epsilon_ui::Tier::Epsilon, proc.info.name, {},
                                                         [] (float h, bool b) { return ui::font (h, b); });
        g.setFont (ui::font (9.5f * s, true, 0.16f)); g.setColour (theme.accent.withAlpha (0.85f));
        g.drawText (theme.tagline, Rectangle<float> (64 * s, 41 * s, 320 * s, 12 * s), Justification::left);
        ignoreUnused (right);
    }
    // panels
    ui::draw::glass (g, Rectangle<float> (28 * s, 88 * s, 444 * s, 240 * s), theme, 8 * s, s);
    ui::draw::recess (g, Rectangle<float> (484 * s, 82 * s, 420 * s, 298 * s), theme, 10 * s, s);
    ui::draw::recess (g, Rectangle<float> (16 * s, 398 * s, 888 * s, 172 * s), theme, 10 * s, s);
    ui::draw::glass (g, spectrumPanel * s, theme, 6 * s, s);
    // captions for bottom-row selectors
    for (int slot : layout.bottom)
        if (auto* c = controlFor (slot); c && (dynamic_cast<ui::Segmented*> (c) != nullptr))
            ui::draw::engraved (g, String (proc.info.spec[size_t (slot)].name).toUpperCase(), c->getBounds().toFloat().translated (0, -18 * s).withHeight (14 * s),
                                Justification::centredLeft, ui::font (9.5f * s, true, 0.2f), theme.inkDim);
    ui::draw::engraved (g, "LATENCY " + String (proc.latencyMilliseconds(), 1) + " MS", Rectangle<float> (spectrumPanel.getX() * s, 548 * s, 300 * s, 14 * s), Justification::left,
                        ui::font (8.5f * s, true, 0.2f), theme.inkFaint);
    for (auto pt : { Point<float> (10, 80), Point<float> (910, 80), Point<float> (10, 580), Point<float> (910, 580) })
        ui::draw::screw (g, pt * s, 4.2f * s, 0.6f + pt.x * 0.01f);
}

void ExpansionEditor::paint (Graphics& g)
{
    const float pixelScale = jmax (1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    if (! background.isValid() || std::abs (pixelScale - backgroundScale) > 0.01f) renderBackground (pixelScale);
    g.setImageResamplingQuality (Graphics::lowResamplingQuality);
    g.drawImage (background, getLocalBounds().toFloat());
    if (! tweakVisible && layout.strip.size() == 1)
        ui::draw::engraved (g, String (proc.info.spec[size_t (layout.strip[0])].name).toUpperCase(),
                            Rectangle<float> (28 * s, 382 * s, 444 * s, 12 * s), Justification::centred, ui::font (9.0f * s, true, 0.25f), theme.inkFaint);
   #if (defined (__clang__) || defined (__GNUC__)) && ! defined (__OPTIMIZE__)
    // An unoptimised (-O0) build runs this DSP many times slower and stutters: say so on the panel.
    g.setColour (Colour (0xffff4a4a)); g.setFont (ui::font (11.0f * s, true, 0.12f));
    g.drawText ("SLOW UNOPTIMISED BUILD - BUILD RELEASE OR SET OPTIMISATION -O3", Rectangle<float> (300 * s, 572 * s, 600 * s, 14 * s), Justification::centredRight);
   #endif
    if (tweakVisible)
    {
        auto r = Rectangle<float> (28 * s, 88 * s, 444 * s, 290 * s);
        ui::draw::recess (g, r, theme, 8 * s, s);
        g.setFont (ui::font (10.5f * s, true, 0.25f)); g.setColour (theme.accent);
        g.drawText ("TWEAK", r.reduced (16 * s, 10 * s), Justification::topLeft);
        g.setFont (ui::font (9.0f * s, true, 0.2f)); g.setColour (theme.inkFaint);
        g.drawText ("ADVANCED CONTROLS", r.reduced (16 * s, 10 * s), Justification::topRight);
        for (int slot : layout.tweak)
            if (auto* c = controlFor (slot); c && dynamic_cast<ui::Segmented*> (c) != nullptr)
                ui::draw::engraved (g, String (proc.info.spec[size_t (slot)].name).toUpperCase(), Rectangle<float> (44 * s, float (c->getY()), 92 * s, float (c->getHeight())),
                                    Justification::centredLeft, ui::font (9.5f * s, true, 0.2f), theme.inkDim);
    }
}
