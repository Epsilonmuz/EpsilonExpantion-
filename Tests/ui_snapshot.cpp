// Renders each editor to PNG after feeding it some music-like audio.
// usage: epsilon_snapshot <outdir> [scale] [effectIndex|-1] [preset] [tweak]
#include "PluginEditor.h"
#include "DSP/Presets.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File dir = juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "snapshots");
    dir.createDirectory();
    const float scale = argc > 2 ? float (std::atof (argv[2])) : 1.0f;
    const int only = argc > 3 ? std::atoi (argv[3]) : -1;
    const int presetIdx = argc > 4 ? std::atoi (argv[4]) : 3;
    const bool tweak = argc > 5 && std::atoi (argv[5]) != 0;
    for (int e = 0; e < 9; ++e)
    {
        if (only >= 0 && e != only) continue;
        ExpansionProcessor proc { eps::Effect (e) };
        proc.setCurrentProgram (presetIdx);
        proc.prepareToPlay (48000, 256);
        juce::AudioBuffer<float> audio (2, 256); juce::MidiBuffer midi;
        ExpansionEditor ed (proc);
        ed.setSize (int (920 * scale), int (590 * scale));
        if (tweak) ed.setTweakVisible (true);
        for (int block = 0; block < 400; ++block)
        {
            for (int i = 0; i < 256; ++i)
            {
                const double t = (block * 256 + i) / 48000.0;
                const double env = std::exp (-6.0 * std::fmod (t, 0.5));
                const double x = env * (0.25 * std::sin (2 * eps::pi * 196 * t) + 0.12 * std::sin (2 * eps::pi * 392.7 * t) + 0.05 * std::sin (2 * eps::pi * 1210 * t));
                audio.setSample (0, i, float (x)); audio.setSample (1, i, float (x * 0.93));
            }
            proc.processBlock (audio, midi);
            if (block % 6 == 0) ed.refreshForSnapshot (1);
        }
        auto img = ed.createComponentSnapshot (ed.getLocalBounds(), true, 1.0f);
        auto f = dir.getChildFile ("Epsilon_" + juce::String (proc.info.name) + (tweak ? "_Tweak" : "") + ".png");
        f.deleteFile();
        juce::FileOutputStream st (f);
        juce::PNGImageFormat().writeImageToStream (img, st);
        std::printf ("%s\n", f.getFullPathName().toRawUTF8());
    }
    return 0;
}
