#pragma once
// Epsilon Expansion 0.2 - parameter definitions for the nine effects.
// Every effect has up to 32 parameter slots. Slots 4 (mix), 5 (output), 14 (bypass) and 15 (tempo)
// have the same meaning in every effect; the rest are effect specific (see descriptor()).
#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>

namespace eps {
constexpr double pi = 3.14159265358979323846;
enum class Effect { Heat, Valve, Crush, Prism, Time, Plate, Space, Tone, Voice };
enum class Unit { Number, Db, Hz, Ms, Seconds, Semitones, Percent, Bpm, Cents, Bipolar };
constexpr int slots = 32, mixSlot = 4, outSlot = 5, bypassSlot = 14, tempoSlot = 15;
using Values = std::array<double, slots>;

struct Spec
{
    const char* id = nullptr;
    const char* name = "";
    double lo = 0, hi = 1, initial = 0;
    Unit unit = Unit::Number;
    const char* choices = nullptr;      // "A|B|C" for choice and toggle parameters
    bool log = false;
};
struct Descriptor
{
    const char* name;                   // "HEAT"
    const char* subtitle;
    const char* code;                   // four-character plugin code
    unsigned colour;                    // accent colour
    std::array<Spec, slots> spec;
};

inline Spec knob (const char* id, const char* name, double lo, double hi, double value, Unit unit = Unit::Number, bool log = false)
{ return { id, name, lo, hi, value, unit, nullptr, log }; }
inline Spec option (const char* id, const char* name, const char* labels, int count, int value = 0)
{ return { id, name, 0, double (count - 1), double (value), Unit::Number, labels, false }; }
inline Spec toggle (const char* id, const char* name, bool value = false) { return option (id, name, "Off|On", 2, int (value)); }

// ------------------------------------------------------------------ named slots per effect
namespace heat  { enum { drive = 0, tone = 1, lowcut = 2, highcut = 3, style = 6, quality = 7, punish = 8, autogain = 9, thump = 10, steep = 11 }; }
namespace valve { enum { input = 0, drive = 1, bass = 2, treble = 3, mic = 6, noise = 7, autogain = 9 }; }
namespace crush { enum { crush = 0, crunch = 1, release = 2, darkness = 3 }; }
namespace prism { enum { pitch = 0, splice = 1, delay = 2, recycle = 3, reverse = 6, sync = 7, note = 8, gateduck = 9, threshold = 10,
                         smoothing = 11, lowcut = 12, highcut = 13, spread = 16, fbmode = 17, offset = 18 }; }
namespace time  { enum { timeA = 0, feedbackA = 1, depth = 2, rate = 3, multiply = 6, freeze = 7, sync = 8, noteA = 9, timeB = 10,
                         feedbackB = 11, levelA = 12, levelB = 13, lowcut = 16, highcut = 17, shape = 18, mode = 19, link = 20, noteB = 21 }; }
namespace plate { enum { decay = 0, predelay = 1, damping = 2, mod = 3, model = 6, freeze = 7, amp = 8, autodecay = 9, lowcut = 10,
                         highcut = 11, width = 12, rate = 13 }; }
namespace space { enum { time = 0, shape = 1, colour = 2, texture = 3, freeze = 6, sync = 7, beats = 8, mod = 9, contour = 10, warp = 11 }; }
namespace tone  { enum { low = 0, mid = 1, high = 2, drive = 3, midfreq = 6, colour = 7 }; }
namespace voice { enum { pitch = 0, formant = 1, drive = 2, tone = 3, mode = 6, range = 7, link = 8 }; }

inline Descriptor descriptor (Effect effect)
{
    using U = Unit;
    Descriptor d {};
    auto& s = d.spec;
    s[mixSlot] = knob ("mix", "Mix", 0, 1, 1, U::Percent);
    s[outSlot] = knob ("output", "Output", -24, 12, 0, U::Db);
    s[bypassSlot] = toggle ("bypass", "Bypass");
    switch (effect)
    {
        case Effect::Heat:
            d = { "HEAT", "Analog saturation", "Eh02", 0xffe8894a, s };
            s[heat::drive]    = knob ("drive", "Drive", 0, 30, 6, U::Db);
            s[heat::tone]     = knob ("tone", "Tone", -6, 6, 0, U::Bipolar);
            s[heat::lowcut]   = knob ("lowcut", "Low Cut", 20, 500, 20, U::Hz, true);
            s[heat::highcut]  = knob ("highcut", "High Cut", 1000, 22000, 22000, U::Hz, true);
            s[heat::style]    = option ("character", "Style", "Tape|Console|Germanium|Triode|Pentode", 5, 0);
            s[heat::quality]  = option ("quality", "Quality", "Eco 2x|High 4x|Ultra 8x", 3, 1);
            s[heat::punish]   = toggle ("punish", "Punish");
            s[heat::autogain] = toggle ("autogain", "Auto", true);
            s[heat::thump]    = toggle ("thump", "Thump");
            s[heat::steep]    = toggle ("steep", "Steep");
            s[mixSlot].initial = 1;
            break;
        case Effect::Valve:
            d = { "VALVE", "Twin-stage tube preamp", "Ev02", 0xffd9b26f, s };
            s[valve::input]    = knob ("input", "Input", 0, 30, 8, U::Db);
            s[valve::drive]    = knob ("drive", "Drive", 0, 24, 4, U::Db);
            s[valve::bass]     = knob ("bass", "Bass", -10, 10, 0, U::Db);
            s[valve::treble]   = knob ("treble", "Treble", -10, 10, 0, U::Db);
            s[valve::mic]      = option ("mic", "Source", "Line|Mic", 2, 0);
            s[valve::noise]    = option ("noise", "Noise", "Clean|Noisy", 2, 0);
            s[valve::autogain] = toggle ("autogain", "Auto Gain", true);
            break;
        case Effect::Crush:
            d = { "CRUSH", "Level destroyer", "Ec02", 0xffe0474c, s };
            s[crush::crush]    = knob ("crush", "Crush", 0, 10, 4, U::Number);
            s[crush::crunch]   = knob ("crunch", "Crunch", 0, 10, 3, U::Number);
            s[crush::release]  = option ("release", "Release", "Slow|Fast", 2, 0);
            s[crush::darkness] = knob ("darkness", "Darkness", 0, 10, 2, U::Number);
            s[mixSlot].initial = 1;
            break;
        case Effect::Prism:
            d = { "PRISM", "Granular echo", "Ep02", 0xffa98bff, s };
            s[prism::pitch]     = knob ("pitch", "Pitch", -24, 24, 12, U::Semitones);
            s[prism::splice]    = knob ("splice", "Splice", 20, 2000, 250, U::Ms, true);
            s[prism::delay]     = knob ("delay", "Delay", 0, 2000, 0, U::Ms);
            s[prism::recycle]   = knob ("recycle", "Recycle", 0, 0.95, 0.3, U::Percent);
            s[prism::reverse]   = option ("reverse", "Direction", "Forward|Reverse", 2, 1);
            s[prism::sync]      = toggle ("sync", "Sync");
            s[prism::note]      = option ("note", "Splice Note", "1/16|1/8|1/8 dotted|1/4|1/4 dotted|1/2|1 bar", 7, 3);
            s[prism::gateduck]  = knob ("gateduck", "Gate / Duck", -1, 1, 0, U::Bipolar);
            s[prism::threshold] = knob ("threshold", "Threshold", -60, 0, -24, U::Db);
            s[prism::smoothing] = knob ("smoothing", "Smoothing", 0, 1, 0.35, U::Percent);
            s[prism::lowcut]    = knob ("lowcut", "Low Cut", 20, 5000, 20, U::Hz, true);
            s[prism::highcut]   = knob ("highcut", "High Cut", 500, 20000, 20000, U::Hz, true);
            s[prism::spread]    = knob ("spread", "Spread", 0, 100, 0, U::Cents);
            s[prism::fbmode]    = option ("fbmode", "Feedback", "Mixed|Dual|Ping-Pong", 3, 0);
            s[prism::offset]    = knob ("offset", "Offset", 0, 0.5, 0, U::Percent);
            s[mixSlot].initial = 0.4;
            s[tempoSlot] = knob ("tempo", "Tempo", 40, 240, 120, U::Bpm);
            break;
        case Effect::Time:
            d = { "TIME", "Variable-clock delay", "Et02", 0xff4fb3d9, s };
            s[time::timeA]     = knob ("time", "Time A", 10, 2500, 250, U::Ms, true);
            s[time::feedbackA] = knob ("feedback", "Feedback A", 0, 1.05, 0.35, U::Percent);
            s[time::depth]     = knob ("depth", "Depth", 0, 1, 0.1, U::Percent);
            s[time::rate]      = knob ("rate", "Rate", 0.1, 256, 0.5, U::Hz, true);
            s[time::multiply]  = option ("multiply", "Multiply", "1x|2x|4x|8x", 4, 0);
            s[time::freeze]    = toggle ("freeze", "Freeze");
            s[time::sync]      = toggle ("sync", "Sync");
            s[time::noteA]     = option ("note", "Note A", "1/16|1/8|1/8 dotted|1/4|1/4 dotted|1/2|1 bar", 7, 3);
            s[time::timeB]     = knob ("timeB", "Time B", 10, 2500, 375, U::Ms, true);
            s[time::feedbackB] = knob ("feedbackB", "Feedback B", 0, 1.05, 0.35, U::Percent);
            s[time::levelA]    = knob ("levelA", "Level A", 0, 1, 1, U::Percent);
            s[time::levelB]    = knob ("levelB", "Level B", 0, 1, 0.8, U::Percent);
            s[time::lowcut]    = knob ("lowcut", "Low Cut", 20, 1000, 20, U::Hz, true);
            s[time::highcut]   = knob ("highcut", "High Cut", 800, 15000, 15000, U::Hz, true);
            s[time::shape]     = option ("shape", "LFO Shape", "Triangle|Sine|Square|Ramp", 4, 0);
            s[time::mode]      = option ("mode", "Mode", "Classic|Ping-Pong|Series|Cross", 4, 0);
            s[time::link]      = toggle ("link", "Link B");
            s[time::noteB]     = option ("noteB", "Note B", "1/16|1/8|1/8 dotted|1/4|1/4 dotted|1/2|1 bar", 7, 4);
            s[mixSlot].initial = 0.35;
            s[tempoSlot] = knob ("tempo", "Tempo", 40, 240, 120, U::Bpm);
            break;
        case Effect::Plate:
            d = { "PLATE", "Electro-mechanical plate", "El02", 0xffd8c08a, s };
            s[plate::decay]     = knob ("decay", "Decay", 0.5, 30, 2.5, U::Seconds, true);
            s[plate::predelay]  = knob ("predelay", "Pre-Delay", 0, 250, 15, U::Ms);
            s[plate::damping]   = knob ("damping", "Damping", 1000, 20000, 9000, U::Hz, true);
            s[plate::mod]       = knob ("mod", "Mod", 0, 1, 0.25, U::Percent);
            s[plate::model]     = option ("model", "Plate", "Steel|Gold Foil|Bright|Warm|Splash", 5, 0);
            s[plate::freeze]    = toggle ("freeze", "Freeze");
            s[plate::amp]       = option ("amp", "Amp", "Clean|Tube|Transistor", 3, 0);
            s[plate::autodecay] = knob ("autodecay", "Auto-Decay", 0, 1, 0, U::Percent);
            s[plate::lowcut]    = knob ("lowcut", "Low Cut", 20, 1000, 60, U::Hz, true);
            s[plate::highcut]   = knob ("highcut", "High Cut", 1000, 20000, 16000, U::Hz, true);
            s[plate::width]     = knob ("width", "Width", 0, 1.5, 1, U::Percent);
            s[plate::rate]      = knob ("rate", "Mod Rate", 0.1, 5, 0.8, U::Hz, true);
            s[mixSlot].initial = 0.3;
            break;
        case Effect::Space:
            d = { "SPACE", "Shaped time tunnels", "Es02", 0xff37c8b4, s };
            s[space::time]    = knob ("time", "Time", 0.1, 60, 4, U::Seconds, true);
            s[space::shape]   = knob ("shape", "Shape", 0, 1, 0.35, U::Percent);
            s[space::colour]  = knob ("colour", "Color", -1, 1, 0, U::Bipolar);
            s[space::texture] = knob ("texture", "Texture", 0, 1, 0.75, U::Percent);
            s[space::freeze]  = toggle ("freeze", "Freeze");
            s[space::sync]    = toggle ("sync", "Sync");
            s[space::beats]   = option ("beats", "Length", "1 beat|2 beats|1 bar|2 bars|4 bars|8 bars", 6, 3);
            s[space::mod]     = knob ("mod", "Mod", 0, 1, 0.3, U::Percent);
            s[space::contour] = knob ("contour", "Contour", 0, 1, 0.6, U::Percent);
            s[space::warp]    = toggle ("warp", "Warp");
            s[mixSlot].initial = 0.35;
            s[tempoSlot] = knob ("tempo", "Tempo", 40, 240, 120, U::Bpm);
            break;
        case Effect::Tone:
            d = { "TONE", "Broadcast console EQ", "Eq02", 0xff8fa9d9, s };
            s[tone::low]     = knob ("low", "Low", -15, 15, 0, U::Db);
            s[tone::mid]     = knob ("mid", "Mid", -8, 8, 0, U::Db);
            s[tone::high]    = knob ("high", "High", -15, 15, 0, U::Db);
            s[tone::drive]   = knob ("drive", "Drive", -15, 15, 0, U::Db);
            s[tone::midfreq] = option ("midfreq", "Mid Freq", "700 Hz|1 kHz|1.5 kHz|2.3 kHz|3.5 kHz|5.6 kHz", 6, 1);
            s[tone::colour]  = toggle ("colour", "Amp", true);
            break;
        case Effect::Voice:
            d = { "VOICE", "Pitch and formant", "Eo02", 0xffe06fd0, s };
            s[voice::pitch]   = knob ("pitch", "Pitch", -12, 12, 0, U::Semitones);
            s[voice::formant] = knob ("formant", "Formant", -12, 12, 0, U::Semitones);
            s[voice::drive]   = knob ("drive", "Drive", 0, 24, 0, U::Db);
            s[voice::tone]    = knob ("tone", "Tone", 1500, 20000, 20000, U::Hz, true);
            s[voice::mode]    = option ("mode", "Mode", "Transpose|Quantize|Robot", 3, 0);
            s[voice::range]   = option ("range", "Voice", "Auto|Low|Mid|High", 4, 0);
            s[voice::link]    = toggle ("link", "Link");
            break;
    }
    return d;
}

inline Values defaults (Effect e) { Values v {}; auto d = descriptor (e); for (size_t i = 0; i < v.size(); ++i) v[i] = d.spec[i].initial; return v; }
inline double checked (const Spec& s, double x)
{ if (! std::isfinite (x)) x = s.initial; x = std::clamp (x, s.lo, s.hi); return s.choices ? std::round (x) : x; }
inline double gain (double db) { return std::exp (db * (std::log (10.0) / 20)); }
inline double toDb (double g) { return 20 * std::log10 (std::max (g, 1e-12)); }
inline double wrap (double x) { return x - std::floor (x); }
inline constexpr std::array<double, 7> noteBeats { { .25, .5, .75, 1, 1.5, 2, 4 } };
inline double beats (double index) { return noteBeats[size_t (std::clamp (int (std::round (index)), 0, 6))]; }
struct Stereo { double l = 0, r = 0; };
inline Stereo blend (Stereo a, Stereo b, double x) { return { a.l + x * (b.l - a.l), a.r + x * (b.r - a.r) }; }
struct Transport { double bpm = 120, ppq = 0; bool hostTempo = false, hasPpq = false, playing = true; };
}
