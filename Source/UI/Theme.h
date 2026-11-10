#pragma once
// Visual identity per effect: one accent hue, a faceplate finish and the glass display tint.
#include <juce_gui_basics/juce_gui_basics.h>
#include "../DSP/Definitions.h"
#include "../../Shared/epsilon_ui/EpsilonHeader.h"
#include "../../Shared/epsilon_ui/EpsilonDialFont.h"

namespace eps::ui {
struct Theme
{
    juce::Colour accent, accent2;           // main and secondary hue (gradients, second trace)
    juce::Colour plateTop, plateBottom;     // faceplate finish
    juce::Colour panel;                     // recessed panels
    juce::Colour glass;                     // display background
    juce::Colour ink = juce::Colour (0xffece6dc), inkDim = juce::Colour (0xff8f8a82), inkFaint = juce::Colour (0xff5b5852);
    bool brushed = true;                    // horizontal brushed-metal grain
    const char* tagline = "";
};

inline Theme themeFor (Effect e)
{
    using C = juce::Colour;
    Theme t;
    switch (e)
    {
        case Effect::Heat:  t = { C (0xffff8a3d), C (0xffffc46b), C (0xff2b2320), C (0xff17120f), C (0xff120e0c), C (0xff0c0807) }; t.tagline = "FIVE-STYLE ANALOG SATURATION"; break;
        case Effect::Valve: t = { C (0xffe9bf6a), C (0xfff3dfa8), C (0xff262722), C (0xff141511), C (0xff0f100d), C (0xff0b0b08) }; t.tagline = "TWIN-STAGE TUBE PREAMP"; break;
        case Effect::Crush: t = { C (0xffff3b47), C (0xffff9a6b), C (0xff1f1d1f), C (0xff0d0c0e), C (0xff0a090b), C (0xff070607) }; t.tagline = "LEVEL DESTROYER"; break;
        case Effect::Prism: t = { C (0xffa98bff), C (0xff63d6ff), C (0xff221f2e), C (0xff100e18), C (0xff0c0a13), C (0xff08070e) }; t.brushed = false; t.tagline = "PITCHED SPLICE ECHO"; break;
        case Effect::Time:  t = { C (0xff4fc3f0), C (0xff9ce8ff), C (0xff1c2530), C (0xff0c1118), C (0xff0a0e14), C (0xff06090d) }; t.tagline = "DUAL VARIABLE-CLOCK DELAY"; break;
        case Effect::Plate: t = { C (0xffe3c58a), C (0xfffff0c9), C (0xff2a2620), C (0xff15120e), C (0xff100e0b), C (0xff0b0907) }; t.tagline = "ELECTRO-MECHANICAL PLATE"; break;
        case Effect::Space: t = { C (0xff3ee0c6), C (0xff7aa8ff), C (0xff16242a), C (0xff091215), C (0xff070f12), C (0xff040a0c) }; t.brushed = false; t.tagline = "SHAPED TIME TUNNELS"; break;
        case Effect::Tone:  t = { C (0xff8fb3ff), C (0xffd0dcff), C (0xff232832), C (0xff11141a), C (0xff0d1015), C (0xff090b0f) }; t.tagline = "BROADCAST CONSOLE EQ"; break;
        case Effect::Voice: t = { C (0xffff6fd8), C (0xffffb36b), C (0xff2a1f2b), C (0xff140e15), C (0xff100b11), C (0xff0b070c) }; t.brushed = false; t.tagline = "PITCH, FORMANT AND ROBOT"; break;
    }
    // Epsilon Sounds design system v1 (Epsilon tier): one shared graphite faceplate for the whole
    // suite; each effect keeps its signature hue only on its display and live controls.
    const auto b = epsilon_ui::palette (epsilon_ui::Tier::Epsilon);
    t.plateTop = b.surface.brighter (0.04f);
    t.plateBottom = b.surface;
    t.panel = b.bg;
    t.glass = b.bg.darker (0.35f).interpolatedWith (t.accent, 0.035f);
    t.ink = b.ink; t.inkDim = b.inkMuted; t.inkFaint = epsilon_ui::faint (b);
    t.brushed = false;                      // flat machined finish, no fake grain
    return t;
}

// Epsilon Dial, the Epsilon Sounds house typeface, embedded so the panels look identical on every machine.
inline juce::Font font (float height, bool bold = false, float kerning = 0.0f)
{
    auto f = epsilon_ui::dial::font (height, bold);
    return kerning != 0.0f ? f.withExtraKerningFactor (kerning) : f;
}
}
