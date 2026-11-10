// Epsilon editors: main-thread cost of one 30 fps frame at Retina scale (timer work + redrawing
// the parts that animate, as macOS asks for it), with the system renderer and with the software-rendered cache.
#include "PluginEditor.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>

static juce::String proc_name (int e) { ExpansionProcessor p { eps::Effect (e) }; return p.getName(); }

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    using Clock = std::chrono::steady_clock;
    std::printf ("| Plugin | System renderer (CoreGraphics on Mac) | Software-rendered cache | Saving |\n|---|---|---|---|\n");
    const int onlyEffect = std::getenv ("EPS_ONLY") ? std::atoi (std::getenv ("EPS_ONLY")) : -1;
    const int onlyMode = std::getenv ("EPS_MODE") ? std::atoi (std::getenv ("EPS_MODE")) : -1;
    for (int e = 0; e < 9; ++e)
    {
        double ms[2] = { 0, 0 };
        if (onlyEffect >= 0 && e != onlyEffect) continue;
        for (int mode = 0; mode < 2; ++mode)
        {
            if (onlyMode >= 0 && mode != onlyMode) continue;
            setenv ("EPSILON_NATIVE_RENDER", mode == 0 ? "1" : "0", 1);
            ExpansionProcessor proc { eps::Effect (e) };
            proc.setCurrentProgram (3);
            proc.prepareToPlay (48000, 512);
            juce::AudioBuffer<float> audio (2, 512); juce::MidiBuffer midi;
            ExpansionEditor ed (proc);
            ed.setVisible (true);
            juce::Image target (juce::Image::ARGB, ed.getWidth() * 2, ed.getHeight() * 2, true, juce::NativeImageType());
            double phase = 0, total = 0; int frames = 0;
            auto* cache = dynamic_cast<eps::ui::SoftwareCachedImage*> (ed.getCachedComponentImage());
            // first draw the whole window once, as opening it would
            { juce::Graphics g (target); g.addTransform (juce::AffineTransform::scale (2.0f)); if (cache) { cache->paint (g); cache->takePendingArea(); } else ed.paintEntireComponent (g, true); }
            const bool silent = std::getenv ("EPS_SILENT") != nullptr;
            for (int f = 0; f < 120; ++f)
            {
                for (int b = 0; b < 3; ++b)
                {
                    for (int i = 0; i < 512; ++i, phase += 1.0 / 48000)
                    { const float x = silent ? 0.0f : float (0.25 * std::sin (2 * eps::pi * 196 * phase) * std::exp (-4 * std::fmod (phase, 0.5)) + 0.08 * std::sin (2 * eps::pi * 1210 * phase)); audio.setSample (0, i, x); audio.setSample (1, i, x); }
                    proc.processBlock (audio, midi);
                }
                const auto t0 = Clock::now();
                // one 30 fps frame = two 60 Hz editor ticks; each tick's repaints reach the window as
                // one merged area (macOS merges a window's dirty rectangles), which is then redrawn.
                for (int tick = 0; tick < 2; ++tick)
                {
                    ed.tickForTest();
                    juce::Rectangle<int> area;
                    if (cache) area = cache->takePendingArea();
                    else for (auto* c : ed.animatedParts()) if (c && c->isVisible()) area = area.isEmpty() ? c->getBoundsInParent() : area.getUnion (c->getBoundsInParent());
                    if (area.isEmpty()) continue;
                    juce::Graphics g (target);
                    g.addTransform (juce::AffineTransform::scale (2.0f));
                    g.reduceClipRegion (area);
                    if (cache) cache->paint (g); else ed.paintEntireComponent (g, true);
                }
                if (f >= 20) { total += std::chrono::duration<double, std::milli> (Clock::now() - t0).count(); ++frames; }
            }
            ms[mode] = total / frames;
        }
        std::printf ("| %s | %.2f ms/frame (%.0f %% of a core) | %.2f ms/frame (%.0f %%) | %.0f %% |\n", proc_name (e).toRawUTF8(),
                     ms[0], ms[0] * 3, ms[1], ms[1] * 3, ms[0] > 0 ? 100 * (1 - ms[1] / ms[0]) : 0.0);
    }
    return 0;
}
