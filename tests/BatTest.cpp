// Offline check of the BAT analysis and suggestion: a dark signal should get
// a top-end boost, a bright one a top-end cut, and silence no suggestion.
#include "../Source/BatAnalysis.h"
#include <cstdio>
#include <random>
#include <vector>

namespace
{
std::vector<float> pinkNoise (int n, unsigned seed)
{
    std::mt19937 rng (seed);
    std::uniform_real_distribution<float> u (-1.0f, 1.0f);
    std::vector<float> out ((size_t) n);
    float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
    for (int i = 0; i < n; ++i)
    {
        const float w = u (rng);
        b0 = 0.99886f * b0 + w * 0.0555179f; b1 = 0.99332f * b1 + w * 0.0750759f;
        b2 = 0.96900f * b2 + w * 0.1538520f; b3 = 0.86650f * b3 + w * 0.3104856f;
        b4 = 0.55000f * b4 + w * 0.5329522f; b5 = -0.7616f * b5 - w * 0.0168980f;
        out[(size_t) i] = 0.05f * (b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362f);
        b6 = w * 0.115926f;
    }
    return out;
}

void lowpass (std::vector<float>& x, float fc, double sr)
{
    // Two one-pole stages: 12 dB per octave above fc.
    const float a = std::exp (-juce::MathConstants<float>::twoPi * fc / (float) sr);
    for (int stage = 0; stage < 2; ++stage)
    {
        float y = 0.0f;
        for (auto& s : x) { y = (1.0f - a) * s + a * y; s = y; }
    }
}

void print (const char* label, const bat::Analysis& a, const bat::Suggestion& s, double sr)
{
    std::printf ("%s  rms %.1f dBFS  crest %.1f dB  heard: %s\n", label, a.rmsDb, a.crestDb,
                 a.heard.joinIntoString (", ").toRawUTF8());
    for (int b = 0; b < bat::numBands; ++b)
        std::printf ("  %-8s %+5.1f dB  %6.0f Hz  Q %.2f\n", bat::bandSpecs()[(size_t) b].name,
                     s.eq[(size_t) b].gainDb, s.eq[(size_t) b].freq, s.eq[(size_t) b].q);
    std::printf ("  level change %+.2f dB\n", bat::weightedLevelDb (a, s.eq, sr) - bat::weightedLevelDb (a, bat::flatSetting(), sr));
}

int failures = 0;
void check (bool ok, const char* what)
{
    std::printf ("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (! ok) ++failures;
}
} // namespace

int main()
{
    const double sr = 48000.0;
    const int n = (int) (sr * 8.0);

    auto dark = pinkNoise (n, 1);
    lowpass (dark, 1500.0f, sr);
    auto bright = pinkNoise (n, 2);
    std::vector<float> silence ((size_t) n, 0.0f);

    const auto aDark = bat::analyse (dark.data(), n, sr);
    const auto aBright = bat::analyse (bright.data(), n, sr);
    const auto aSilent = bat::analyse (silence.data(), n, sr);

    const auto sDark = bat::suggest (aDark, bat::Mode::Neutral, sr);
    const auto sBright = bat::suggest (aBright, bat::Mode::Neutral, sr);
    print ("dark", aDark, sDark, sr);
    print ("bright", aBright, sBright, sr);

    const auto topDark = bat::eqResponseDb (sDark.eq, 10000.0f, sr);
    const auto topBright = bat::eqResponseDb (sBright.eq, 10000.0f, sr);

    check (aDark.valid && aBright.valid, "noise is analysed");
    check (! aSilent.valid, "silence gives no analysis");
    check (aDark.heard.contains ("Dull top end"), "dark signal heard as dull");
    check (topDark > 1.0f, "dark signal gets a top-end boost");
    check (topBright < -1.0f, "bright signal gets a top-end cut");

    for (int m = 0; m < bat::numModes; ++m)
    {
        const auto s = bat::suggest (aDark, (bat::Mode) m, sr);
        bool inRange = true;
        for (auto& band : s.eq)
            inRange = inRange && std::abs (band.gainDb) <= 5.0f && std::isfinite (band.freq);
        check (inRange, ("mode " + bat::modeName ((bat::Mode) m) + " stays within limits").toRawUTF8());
    }

    const auto hw = bat::suggest (aBright, bat::Mode::Hardwell, sr);
    const auto nt = bat::suggest (aBright, bat::Mode::Neutral, sr);
    check (bat::eqResponseDb (hw.eq, 50.0f, sr) > bat::eqResponseDb (nt.eq, 50.0f, sr),
           "Hardwell mode adds more sub than Neutral");

    std::printf (failures == 0 ? "All BAT checks passed.\n" : "%d BAT check(s) failed.\n", failures);
    return failures == 0 ? 0 : 1;
}
