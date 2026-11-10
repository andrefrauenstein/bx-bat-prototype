# bx_BAT EQ (prototype)

Brainworx Assistive Technology (BAT) in a 5-band EQ laid out like the AMEK EQ 250.
BAT listens to the input, compares what it heard with the target curve of an engineer
mode, and loads a suggested setting into slot C so it can be compared with the user's slot A.

- **Listen**: 8 seconds of non-silent input, taken before the EQ (pauses while the track is silent).
- **Analyse**: averaged FFT, energy per 1/6 octave on a 96-point grid, normalised to 200 Hz–5 kHz.
  Tags for what it heard: low end, low mids, top end, density (crest factor).
- **Engineer modes**: Neutral, Clearmountain, Hardwell, Brainworx. Each is a target curve,
  a reference curve plus a mode offset (`Source/BatAnalysis.cpp`). The engineer curves are
  placeholders until they are defined with the engineers.
- **Suggest**: closes 60% of the gap to the target. Shelves first, then bells, each band fitted by
  least squares over its own frequency range, with a bias towards small moves; two passes.
  Limits ±4 dB on shelves and ±3 dB on bells, 0.5 dB steps.
- **Compare**: A/B/C/D slots. The suggestion goes to C; knobs that BAT moved glow. Level match
  sets C's output gain to the same loudness as A, estimated from the analysed spectrum.

## Build

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBX_BUILD_TESTS=ON
cmake --build build --target BxBat_VST3 BxBat_AU BatTest
```

JUCE 8.0.4 is fetched automatically. Pushing to `main` builds a universal macOS VST3 + AU on
GitHub Actions; the zip lands on the `builds` branch and as a workflow artifact.

`tests/BatTest.cpp` checks the analysis offline: a dark signal gets a top-end boost, a bright one a
cut, silence gives no suggestion, every mode stays within its limits, and Hardwell adds more sub
than Neutral.
