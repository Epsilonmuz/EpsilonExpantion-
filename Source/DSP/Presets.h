#pragma once
// Factory presets: hand-set starting points, 12 per effect. Each preset lists only the controls it changes
// from the effect's defaults. Output trims keep presets at roughly the same loudness as bypass.
#include "Definitions.h"
#include <initializer_list>
#include <utility>

namespace eps {
struct PresetDef { const char* name; std::initializer_list<std::pair<int, double>> set; };
constexpr int presetCount = 12;

inline const PresetDef& presetDef (Effect e, int index)
{
    using namespace std;
    static const PresetDef heatP[presetCount] = {
        { "Default", {} },
        { "Vocal Warmth", { { heat::style, 0 }, { heat::drive, 9 }, { heat::lowcut, 90 }, { heat::tone, 0.8 }, { mixSlot, 0.7 } } },
        { "Drum Bus Bite", { { heat::style, 3 }, { heat::drive, 14 }, { heat::thump, 1 }, { heat::lowcut, 45 }, { mixSlot, 0.45 } } },
        { "Bass Weight", { { heat::style, 2 }, { heat::drive, 12 }, { heat::lowcut, 35 }, { heat::thump, 1 }, { heat::highcut, 6500 } } },
        { "Console Glue", { { heat::style, 1 }, { heat::drive, 6 }, { mixSlot, 1 } } },
        { "Snare Crack", { { heat::style, 4 }, { heat::drive, 16 }, { heat::tone, 2 }, { mixSlot, 0.5 } } },
        { "Guitar Cab", { { heat::style, 4 }, { heat::drive, 22 }, { heat::lowcut, 110 }, { heat::highcut, 4500 }, { heat::steep, 1 } } },
        { "AM Radio", { { heat::style, 2 }, { heat::drive, 18 }, { heat::lowcut, 450 }, { heat::highcut, 3200 }, { heat::steep, 1 } } },
        { "Parallel Grit", { { heat::style, 4 }, { heat::drive, 24 }, { heat::punish, 1 }, { mixSlot, 0.25 } } },
        { "Tape Polish", { { heat::style, 0 }, { heat::drive, 5 }, { heat::tone, -0.8 }, { heat::highcut, 16000 } } },
        { "808 Growl", { { heat::style, 3 }, { heat::drive, 20 }, { heat::lowcut, 25 }, { heat::thump, 1 }, { heat::highcut, 7000 }, { mixSlot, 0.6 } } },
        { "Destroy", { { heat::style, 4 }, { heat::drive, 30 }, { heat::punish, 1 }, { heat::tone, 3 } } } };
    static const PresetDef valveP[presetCount] = {
        { "Default", {} },
        { "Vocal Glow", { { valve::input, 10 }, { valve::drive, 4 }, { valve::treble, 3 }, { valve::bass, -2 }, { valve::mic, 1 } } },
        { "Warm Keys", { { valve::input, 12 }, { valve::drive, 6 }, { valve::bass, 2 }, { valve::treble, -2 } } },
        { "Bass DI", { { valve::input, 14 }, { valve::drive, 8 }, { valve::bass, 4 }, { valve::treble, -3 } } },
        { "Drum Room", { { valve::input, 18 }, { valve::drive, 12 }, { valve::treble, 4 }, { mixSlot, 0.5 } } },
        { "Old Broadcast", { { valve::input, 16 }, { valve::drive, 10 }, { valve::bass, -6 }, { valve::treble, -4 }, { valve::noise, 1 }, { valve::mic, 1 } } },
        { "Hot Second Stage", { { valve::input, 6 }, { valve::drive, 20 } } },
        { "Gentle Line", { { valve::input, 4 }, { valve::drive, 2 } } },
        { "Fat Snare", { { valve::input, 20 }, { valve::drive, 8 }, { valve::bass, 3 }, { mixSlot, 0.6 } } },
        { "Air Lift", { { valve::input, 8 }, { valve::drive, 3 }, { valve::treble, 7 } } },
        { "Noisy Mic Pre", { { valve::input, 22 }, { valve::drive, 6 }, { valve::mic, 1 }, { valve::noise, 1 } } },
        { "Full Saturation", { { valve::input, 30 }, { valve::drive, 24 } } } };
    static const PresetDef crushP[presetCount] = {
        { "Default", {} },
        { "Room Mic Smash", { { crush::crush, 8 }, { crush::crunch, 2 }, { crush::darkness, 3 }, { mixSlot, 0.5 } } },
        { "Parallel Drums", { { crush::crush, 7 }, { crush::crunch, 4 }, { crush::release, 1 }, { mixSlot, 0.35 } } },
        { "Dark Pump", { { crush::crush, 9 }, { crush::crunch, 1 }, { crush::darkness, 7 }, { crush::release, 1 } } },
        { "Bass Lock", { { crush::crush, 5 }, { crush::crunch, 3 }, { crush::darkness, 5 } } },
        { "Lo-Fi Vocal", { { crush::crush, 6 }, { crush::crunch, 6 }, { crush::darkness, 6 }, { mixSlot, 0.7 } } },
        { "Gentle Level", { { crush::crush, 2 }, { crush::crunch, 0 }, { crush::darkness, 0 } } },
        { "Snare Explosion", { { crush::crush, 10 }, { crush::crunch, 7 }, { crush::release, 1 }, { mixSlot, 0.4 } } },
        { "Telephone", { { crush::crush, 6 }, { crush::crunch, 8 }, { crush::darkness, 8 } } },
        { "Ambience Lift", { { crush::crush, 9 }, { crush::crunch, 0 }, { crush::darkness, 2 }, { mixSlot, 0.25 } } },
        { "Breakbeat", { { crush::crush, 7 }, { crush::crunch, 5 }, { crush::release, 1 }, { crush::darkness, 4 } } },
        { "Full Destruction", { { crush::crush, 10 }, { crush::crunch, 10 }, { crush::darkness, 3 } } } };
    static const PresetDef prismP[presetCount] = {
        { "Default", {} },
        { "Octave Shimmer", { { prism::pitch, 12 }, { prism::reverse, 0 }, { prism::splice, 180 }, { prism::recycle, 0.55 }, { prism::smoothing, 0.6 }, { prism::spread, 12 } } },
        { "Reverse Fifths", { { prism::pitch, 7 }, { prism::reverse, 1 }, { prism::splice, 350 }, { prism::recycle, 0.4 } } },
        { "Down Under", { { prism::pitch, -12 }, { prism::reverse, 1 }, { prism::splice, 420 }, { prism::recycle, 0.3 }, { prism::highcut, 6000 } } },
        { "Crystal Stairs", { { prism::pitch, 5 }, { prism::reverse, 0 }, { prism::splice, 120 }, { prism::recycle, 0.7 }, { prism::fbmode, 2 } } },
        { "Vocal Halo", { { prism::pitch, 12 }, { prism::reverse, 1 }, { prism::splice, 250 }, { prism::gateduck, 0.6 }, { prism::threshold, -30 }, { mixSlot, 0.3 } } },
        { "Synced Reverse", { { prism::pitch, 0 }, { prism::reverse, 1 }, { prism::sync, 1 }, { prism::note, 3 }, { prism::recycle, 0.35 } } },
        { "Glitch Field", { { prism::pitch, 19 }, { prism::splice, 40 }, { prism::smoothing, 0.05 }, { prism::recycle, 0.5 }, { prism::offset, 0.3 } } },
        { "Wide Detune", { { prism::pitch, 0 }, { prism::reverse, 0 }, { prism::splice, 90 }, { prism::spread, 40 }, { prism::recycle, 0.2 }, { mixSlot, 0.35 } } },
        { "Spiral Up", { { prism::pitch, 7 }, { prism::reverse, 0 }, { prism::splice, 300 }, { prism::delay, 120 }, { prism::recycle, 0.8 }, { prism::fbmode, 2 } } },
        { "Gated Sparkle", { { prism::pitch, 24 }, { prism::splice, 150 }, { prism::gateduck, -0.8 }, { prism::threshold, -24 }, { prism::lowcut, 900 } } },
        { "Deep Reverse Pad", { { prism::pitch, -24 }, { prism::reverse, 1 }, { prism::splice, 900 }, { prism::smoothing, 0.9 }, { prism::recycle, 0.6 }, { prism::highcut, 4000 } } } };
    static const PresetDef timeP[presetCount] = {
        { "Default", {} },
        { "Slapback", { { time::timeA, 95 }, { time::timeB, 125 }, { time::feedbackA, 0.1 }, { time::feedbackB, 0.1 }, { mixSlot, 0.3 } } },
        { "Dotted Ping-Pong", { { time::sync, 1 }, { time::noteA, 2 }, { time::noteB, 2 }, { time::mode, 1 }, { time::feedbackA, 0.55 } } },
        { "Dark 2x Memory", { { time::multiply, 1 }, { time::timeA, 300 }, { time::timeB, 450 }, { time::feedbackA, 0.55 }, { time::feedbackB, 0.55 } } },
        { "Lo-Fi 8x Loop", { { time::multiply, 3 }, { time::timeA, 220 }, { time::link, 1 }, { time::feedbackA, 0.7 }, { time::feedbackB, 0.7 } } },
        { "Flanged Repeats", { { time::timeA, 180 }, { time::depth, 0.6 }, { time::rate, 0.3 }, { time::feedbackA, 0.6 }, { time::feedbackB, 0.6 } } },
        { "Audio-Rate Warble", { { time::timeA, 30 }, { time::timeB, 45 }, { time::depth, 0.5 }, { time::rate, 90 }, { time::shape, 1 } } },
        { "Runaway", { { time::timeA, 350 }, { time::feedbackA, 1.02 }, { time::feedbackB, 1.0 }, { time::highcut, 3500 }, { time::lowcut, 200 } } },
        { "Series Cascade", { { time::mode, 2 }, { time::timeA, 140 }, { time::timeB, 210 }, { time::feedbackA, 0.5 } } },
        { "Criss-Cross", { { time::mode, 3 }, { time::timeA, 240 }, { time::timeB, 360 }, { time::feedbackA, 0.5 }, { time::feedbackB, 0.5 } } },
        { "Telephone Echo", { { time::timeA, 260 }, { time::lowcut, 600 }, { time::highcut, 2500 }, { time::feedbackA, 0.45 }, { time::feedbackB, 0.45 } } },
        { "Square Chorus", { { time::timeA, 12 }, { time::timeB, 17 }, { time::depth, 0.4 }, { time::rate, 1.2 }, { time::shape, 2 }, { mixSlot, 0.5 } } } };
    static const PresetDef plateP[presetCount] = {
        { "Default", {} },
        { "Vocal Plate", { { plate::model, 0 }, { plate::decay, 1.8 }, { plate::predelay, 35 }, { plate::lowcut, 150 }, { plate::damping, 7000 } } },
        { "Snare Plate", { { plate::model, 2 }, { plate::decay, 1.4 }, { plate::predelay, 10 }, { plate::lowcut, 200 } } },
        { "Gold Foil Tight", { { plate::model, 1 }, { plate::decay, 0.9 }, { plate::predelay, 5 } } },
        { "Warm Ballad", { { plate::model, 3 }, { plate::decay, 3.2 }, { plate::predelay, 40 }, { plate::damping, 5500 } } },
        { "Splash Cymbal", { { plate::model, 4 }, { plate::decay, 2.2 }, { plate::predelay, 0 } } },
        { "Huge Tail", { { plate::model, 0 }, { plate::decay, 9 }, { plate::predelay, 60 }, { plate::mod, 0.5 } } },
        { "Ducked Vocal", { { plate::model, 0 }, { plate::decay, 3 }, { plate::autodecay, 0.8 }, { plate::predelay, 30 } } },
        { "Tube Plate", { { plate::model, 3 }, { plate::amp, 1 }, { plate::decay, 2.6 } } },
        { "Bright Transistor", { { plate::model, 2 }, { plate::amp, 2 }, { plate::decay, 2.0 }, { plate::highcut, 18000 } } },
        { "Narrow Room", { { plate::model, 1 }, { plate::decay, 1.2 }, { plate::width, 0.4 } } },
        { "Infinite Pad", { { plate::model, 0 }, { plate::decay, 30 }, { plate::mod, 0.6 }, { plate::damping, 6000 }, { mixSlot, 0.5 } } } };
    static const PresetDef spaceP[presetCount] = {
        { "Default", {} },
        { "Reverse Swell", { { space::time, 3 }, { space::shape, 1 }, { space::contour, 0.9 }, { space::texture, 0.8 } } },
        { "Gated Room", { { space::time, 0.6 }, { space::shape, 0.2 }, { space::contour, 0.0 }, { space::texture, 0.9 } } },
        { "Bloom Cloud", { { space::time, 6 }, { space::shape, 0.5 }, { space::contour, 0.8 }, { space::mod, 0.5 } } },
        { "Discrete Echo Field", { { space::time, 2.5 }, { space::shape, 0.1 }, { space::texture, 0.05 } } },
        { "Bar-Length Swell", { { space::sync, 1 }, { space::beats, 2 }, { space::shape, 0.85 }, { space::contour, 0.8 } } },
        { "Dark Tunnel", { { space::time, 10 }, { space::colour, -0.7 }, { space::shape, 0.3 } } },
        { "Bright Rise", { { space::time, 5 }, { space::colour, 0.8 }, { space::shape, 0.8 } } },
        { "One-Minute Space", { { space::time, 60 }, { space::shape, 0.45 }, { space::contour, 0.7 }, { space::texture, 1 }, { mixSlot, 0.3 } } },
        { "Tape Warp Hall", { { space::time, 4 }, { space::warp, 1 }, { space::mod, 0.6 } } },
        { "Snare Gate", { { space::time, 0.35 }, { space::shape, 0.15 }, { space::contour, 0.1 }, { space::texture, 1 }, { mixSlot, 0.4 } } },
        { "Ambient Drift", { { space::time, 20 }, { space::shape, 0.55 }, { space::contour, 0.6 }, { space::mod, 0.8 }, { space::colour, -0.3 } } } };
    static const PresetDef toneP[presetCount] = {
        { "Default", {} },
        { "Vocal Presence", { { tone::mid, 3 }, { tone::midfreq, 3 }, { tone::high, 4 }, { tone::low, -3 } } },
        { "Kick Body", { { tone::low, 6 }, { tone::mid, -4 }, { tone::midfreq, 0 }, { tone::drive, 4 } } },
        { "Snare Snap", { { tone::mid, 4 }, { tone::midfreq, 2 }, { tone::high, 3 }, { tone::drive, 6 } } },
        { "Bus Air", { { tone::high, 5 }, { tone::drive, 2 } } },
        { "Telephone", { { tone::low, -15 }, { tone::high, -15 }, { tone::mid, 8 }, { tone::midfreq, 1 } } },
        { "Warm Master", { { tone::low, 2 }, { tone::high, -2 }, { tone::drive, 3 } } },
        { "Guitar Focus", { { tone::low, -4 }, { tone::mid, 3 }, { tone::midfreq, 2 }, { tone::drive, 5 } } },
        { "Dark Keys", { { tone::high, -6 }, { tone::mid, -2 }, { tone::midfreq, 4 } } },
        { "Hot Console", { { tone::drive, 12 } } },
        { "Clean Shelf", { { tone::colour, 0 }, { tone::low, 3 }, { tone::high, 3 } } },
        { "Mid Scoop", { { tone::mid, -6 }, { tone::midfreq, 1 }, { tone::low, 2 }, { tone::high, 2 } } } };
    static const PresetDef voiceP[presetCount] = {
        { "Default", {} },
        { "Octave Down Monster", { { voice::pitch, -12 }, { voice::formant, -4 } } },
        { "Chipmunk", { { voice::pitch, 12 }, { voice::link, 1 } } },
        { "Hard Tune", { { voice::mode, 1 } } },
        { "Robot C", { { voice::mode, 2 }, { voice::pitch, 0 }, { voice::drive, 6 } } },
        { "Fifth Harmony", { { voice::pitch, 7 }, { mixSlot, 0.45 } } },
        { "Deeper Voice", { { voice::formant, -3 } } },
        { "Brighter Voice", { { voice::formant, 3 } } },
        { "Radio Robot", { { voice::mode, 2 }, { voice::pitch, 7 }, { voice::drive, 12 }, { voice::tone, 4000 } } },
        { "Lower Third", { { voice::pitch, -4 }, { mixSlot, 0.5 } } },
        { "Tape Slowdown", { { voice::pitch, -5 }, { voice::link, 1 } } },
        { "Gritty Double", { { voice::pitch, -12 }, { voice::drive, 10 }, { mixSlot, 0.4 } } } };
    static const PresetDef* all[9] = { heatP, valveP, crushP, prismP, timeP, plateP, spaceP, toneP, voiceP };
    return all[std::clamp (int (e), 0, 8)][std::clamp (index, 0, presetCount - 1)];
}
inline const char* presetName (Effect e, int index) { return presetDef (e, index).name; }
inline Values preset (Effect e, int index)
{
    auto v = defaults (e);
    for (const auto& kv : presetDef (e, index).set) v[size_t (kv.first)] = kv.second;
    auto d = descriptor (e);
    for (size_t i = 0; i < v.size(); ++i) if (d.spec[i].id) v[i] = checked (d.spec[i], v[i]);
    return v;
}
}
