#include "PluginProcessor.h"
#ifndef EPSILON_EFFECT_INDEX
#define EPSILON_EFFECT_INDEX 0
#endif
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new ExpansionProcessor(eps::Effect(EPSILON_EFFECT_INDEX));}
