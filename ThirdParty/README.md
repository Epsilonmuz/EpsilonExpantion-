# Third-party source

Signalsmith Stretch version 1.3.2, MIT, copyright Signalsmith Audio:
https://github.com/Signalsmith-Audio/signalsmith-stretch
Pinned commit: 88061fd4eda56a2353c5d65c74aa3f4ccf76f9da

Signalsmith Linear, MIT:
https://github.com/Signalsmith-Audio/linear
Pinned commit: 7bb1c8f36794d6980080ce817c91f8da8fe56a66

One local portability change makes the Stretch seed conversion to the random engine's result_type explicit, resolving the Apple Clang integer precision warning.
Other library code is unchanged. Both complete MIT notices are
included in this directory. Voice uses Stretch for spectral pitch/formant processing.
The robot vocoder, wrapper, oversampling, colour, compression, delay and FDN code are
separate Epsilon implementations.

JUCE 8.0.12 is fetched or supplied locally; it is not vendored in this archive.
