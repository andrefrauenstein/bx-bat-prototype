#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>

// Brainworx Assistive Technology (BAT): analysis of the incoming audio and
// a suggested starting setting for a 5-band EQ, per "engineer mode".
// Pure functions, no plugin state, so it can be tested offline.
namespace bat
{
enum BandType { LowShelf, Peak, HighShelf };
constexpr int numBands = 5;

struct BandSpec
{
    const char* name;
    BandType type;
    float fMin, fMax, fDefault, qDefault;
};
const std::array<BandSpec, numBands>& bandSpecs();

struct BandSetting
{
    float gainDb = 0.0f, freq = 1000.0f, q = 0.8f;
};
using EqSetting = std::array<BandSetting, numBands>;
EqSetting flatSetting();

enum class Mode { Neutral = 0, Clearmountain, Hardwell, Brainworx };
constexpr int numModes = 4;
juce::String modeName (Mode);
juce::String modeDescription (Mode);

// Log-spaced analysis grid, 20 Hz to 20 kHz.
constexpr int gridSize = 96;
float gridFreq (int i);
using Curve = std::array<float, gridSize>;

struct Analysis
{
    bool valid = false;
    Curve spectrumDb {};   // energy per 1/6 octave, 0 dB = mean of 200 Hz to 5 kHz
    Curve powerLin {};     // relative energy per grid point, for level matching
    float rmsDb = -100.0f, peakDb = -100.0f, crestDb = 0.0f;
    juce::StringArray heard;
};

// mono: the captured, non-silent audio. Needs at least ~0.1 s.
Analysis analyse (const float* mono, int numSamples, double sampleRate);

// The target tonal balance for a mode, normalised like Analysis::spectrumDb.
// These targets are placeholders until they are defined with the engineers.
Curve targetCurve (Mode);

std::array<float, 6> coefficientsFor (const BandSpec&, const BandSetting&, double sampleRate);
float bandResponseDb (const BandSpec&, const BandSetting&, float freq, double sampleRate);
float eqResponseDb (const EqSetting&, float freq, double sampleRate);

// Loudness of the analysed audio after an EQ, from its spectrum (dB, relative).
float weightedLevelDb (const Analysis&, const EqSetting&, double sampleRate);

struct Suggestion
{
    EqSetting eq;
    Curve eqCurveDb {};
};

// strength: how much of the gap to the target the EQ closes (0..1).
Suggestion suggest (const Analysis&, Mode, double sampleRate, float strength = 0.6f);
} // namespace bat
