#include "BatAnalysis.h"
#include <cmath>
#include <complex>
#include <vector>

namespace bat
{
const std::array<BandSpec, numBands>& bandSpecs()
{
    static const std::array<BandSpec, numBands> specs { {
        { "LO SHELF", LowShelf,  20.0f,   620.0f,   100.0f, 0.707f },
        { "LOW",      Peak,      20.0f,   620.0f,   200.0f, 0.8f },
        { "MID",      Peak,      120.0f,  6800.0f,  970.0f, 0.8f },
        { "HIGH",     Peak,      400.0f,  20000.0f, 3000.0f, 0.8f },
        { "HI SHELF", HighShelf, 400.0f,  20000.0f, 10000.0f, 0.707f },
    } };
    return specs;
}

EqSetting flatSetting()
{
    EqSetting s;
    for (int b = 0; b < numBands; ++b)
        s[(size_t) b] = { 0.0f, bandSpecs()[(size_t) b].fDefault, bandSpecs()[(size_t) b].qDefault };
    return s;
}

juce::String modeName (Mode m)
{
    switch (m)
    {
        case Mode::Neutral:       return "Neutral";
        case Mode::Clearmountain: return "Clearmountain";
        case Mode::Hardwell:      return "Hardwell";
        case Mode::Brainworx:     return "Brainworx";
    }
    return {};
}

juce::String modeDescription (Mode m)
{
    switch (m)
    {
        case Mode::Neutral:       return "Balances the track against a reference curve.";
        case Mode::Clearmountain: return "Placeholder target: open top, firm low end. Not Bob Clearmountain's own settings.";
        case Mode::Hardwell:      return "Placeholder target: big sub, bright top, tucked mids. Not Hardwell's own settings.";
        case Mode::Brainworx:     return "Placeholder house target: warm low mids, smooth top.";
    }
    return {};
}

float gridFreq (int i)
{
    return 20.0f * std::pow (1000.0f, (float) i / (float) (gridSize - 1));
}

namespace
{
using Points = std::vector<std::pair<float, float>>;

float interp (const Points& pts, float f)
{
    if (f <= pts.front().first) return pts.front().second;
    if (f >= pts.back().first)  return pts.back().second;

    for (size_t k = 1; k < pts.size(); ++k)
    {
        if (f <= pts[k].first)
        {
            const float l0 = std::log (pts[k - 1].first), l1 = std::log (pts[k].first);
            const float t = (std::log (f) - l0) / (l1 - l0);
            return pts[k - 1].second + t * (pts[k].second - pts[k - 1].second);
        }
    }
    return pts.back().second;
}

void normalise (Curve& c)
{
    double sum = 0.0;
    int n = 0;
    for (int i = 0; i < gridSize; ++i)
    {
        const float f = gridFreq (i);
        if (f >= 200.0f && f <= 5000.0f) { sum += c[(size_t) i]; ++n; }
    }
    const float mean = n > 0 ? (float) (sum / n) : 0.0f;
    for (auto& v : c) v -= mean;
}

// Energy per 1/6 octave of a typical modern master (pink noise is flat here).
const Points& neutralPoints()
{
    static const Points p { { 25.0f, -4.0f }, { 40.0f, 3.0f }, { 60.0f, 5.0f }, { 100.0f, 5.0f },
                            { 200.0f, 3.0f }, { 400.0f, 1.5f }, { 1000.0f, 0.0f }, { 2000.0f, -1.5f },
                            { 4000.0f, -4.0f }, { 8000.0f, -7.5f }, { 12000.0f, -10.5f },
                            { 16000.0f, -14.0f }, { 20000.0f, -20.0f } };
    return p;
}

const Points& modeOffsets (Mode m)
{
    static const Points none { { 20.0f, 0.0f }, { 20000.0f, 0.0f } };
    static const Points clearmountain { { 20.0f, 0.0f }, { 60.0f, 1.0f }, { 150.0f, 0.5f }, { 300.0f, -0.5f },
                                        { 1000.0f, 0.0f }, { 3000.0f, 0.5f }, { 10000.0f, 1.5f }, { 16000.0f, 2.0f } };
    static const Points hardwell { { 20.0f, 0.0f }, { 50.0f, 2.5f }, { 100.0f, 1.5f }, { 250.0f, 0.0f },
                                   { 500.0f, -1.0f }, { 1000.0f, -0.5f }, { 2500.0f, 0.0f }, { 5000.0f, 1.0f },
                                   { 12000.0f, 2.5f } };
    static const Points brainworx { { 20.0f, 0.0f }, { 80.0f, 1.0f }, { 250.0f, 1.0f }, { 600.0f, 0.0f },
                                    { 3000.0f, -0.5f }, { 10000.0f, 0.5f }, { 16000.0f, 0.0f } };
    switch (m)
    {
        case Mode::Neutral:       return none;
        case Mode::Clearmountain: return clearmountain;
        case Mode::Hardwell:      return hardwell;
        case Mode::Brainworx:     return brainworx;
    }
    return none;
}

float meanOver (const Curve& c, float lo, float hi)
{
    double s = 0.0;
    int n = 0;
    for (int i = 0; i < gridSize; ++i)
    {
        const float f = gridFreq (i);
        if (f >= lo && f <= hi) { s += c[(size_t) i]; ++n; }
    }
    return n > 0 ? (float) (s / n) : 0.0f;
}
} // namespace

Curve targetCurve (Mode m)
{
    Curve c {};
    for (int i = 0; i < gridSize; ++i)
    {
        const float f = gridFreq (i);
        c[(size_t) i] = interp (neutralPoints(), f) + interp (modeOffsets (m), f);
    }
    normalise (c);
    return c;
}

Analysis analyse (const float* x, int n, double sr)
{
    Analysis a;
    constexpr int order = 12, N = 1 << order, hop = N / 2;
    if (x == nullptr || n < N || sr <= 0.0)
        return a;

    juce::dsp::FFT fft (order);
    std::vector<float> win ((size_t) N), buf ((size_t) N * 2), acc ((size_t) N / 2 + 1, 0.0f);
    for (int i = 0; i < N; ++i)
        win[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (N - 1));

    int frames = 0;
    double sumSq = 0.0;
    long counted = 0;
    float peak = 0.0f;

    for (int i = 0; i < n; ++i)
        peak = std::max (peak, std::abs (x[i]));

    for (int start = 0; start + N <= n; start += hop)
    {
        double e = 0.0;
        for (int i = 0; i < N; ++i)
            e += (double) x[start + i] * x[start + i];
        if (std::sqrt (e / N) < 0.001)   // below -60 dBFS: skip
            continue;

        for (int i = 0; i < hop; ++i)
            sumSq += (double) x[start + i] * x[start + i];
        counted += hop;

        std::fill (buf.begin(), buf.end(), 0.0f);
        for (int i = 0; i < N; ++i)
            buf[(size_t) i] = x[start + i] * win[(size_t) i];
        fft.performFrequencyOnlyForwardTransform (buf.data(), true);
        for (int k = 0; k <= N / 2; ++k)
            acc[(size_t) k] += buf[(size_t) k] * buf[(size_t) k];
        ++frames;
    }

    if (frames == 0)
        return a;

    const double binHz = sr / N;
    const float edge = std::pow (2.0f, 1.0f / 12.0f);   // +-1/12 octave = 1/6-octave band
    int lastValid = -1;

    for (int i = 0; i < gridSize; ++i)
    {
        const float f = gridFreq (i);
        if (f > 0.47f * (float) sr)
        {
            a.spectrumDb[(size_t) i] = lastValid >= 0 ? a.spectrumDb[(size_t) lastValid] : -100.0f;
            a.powerLin[(size_t) i] = 0.0f;
            continue;
        }

        const int k0 = (int) std::ceil (f / edge / binHz);
        const int k1 = (int) std::floor (f * edge / binHz);
        double power = 0.0;
        if (k1 >= k0)
        {
            for (int k = std::max (1, k0); k <= std::min (N / 2, k1); ++k)
                power += acc[(size_t) k];
        }
        else
        {
            // Band narrower than one bin: scale the nearest bin by the band's width.
            const int k = juce::jlimit (1, N / 2, (int) std::lround (f / binHz));
            power = acc[(size_t) k] * (f * edge - f / edge) / binHz;
        }
        power /= frames;
        a.powerLin[(size_t) i] = (float) power;
        a.spectrumDb[(size_t) i] = 10.0f * std::log10 ((float) power + 1.0e-20f);
        lastValid = i;
    }

    // Light smoothing across the grid.
    Curve smoothed = a.spectrumDb;
    for (int i = 1; i < gridSize - 1; ++i)
        smoothed[(size_t) i] = 0.25f * a.spectrumDb[(size_t) i - 1] + 0.5f * a.spectrumDb[(size_t) i]
                             + 0.25f * a.spectrumDb[(size_t) i + 1];
    a.spectrumDb = smoothed;
    normalise (a.spectrumDb);

    double totalPower = 0.0;
    for (auto p : a.powerLin) totalPower += p;
    if (totalPower > 0.0)
        for (auto& p : a.powerLin) p = (float) (p / totalPower);

    a.rmsDb = counted > 0 ? juce::Decibels::gainToDecibels ((float) std::sqrt (sumSq / counted), -100.0f) : -100.0f;
    a.peakDb = juce::Decibels::gainToDecibels (peak, -100.0f);
    a.crestDb = a.peakDb - a.rmsDb;
    a.valid = true;

    // What BAT heard, compared with the neutral reference.
    const Curve ref = targetCurve (Mode::Neutral);
    Curve diff {};
    for (int i = 0; i < gridSize; ++i)
        diff[(size_t) i] = a.spectrumDb[(size_t) i] - ref[(size_t) i];

    const float lows = meanOver (diff, 40.0f, 120.0f);
    const float boxy = meanOver (diff, 250.0f, 600.0f);
    const float top = meanOver (diff, 6000.0f, std::min (14000.0f, 0.45f * (float) sr));

    if (lows > 2.5f)  a.heard.add ("Heavy low end");
    if (lows < -2.5f) a.heard.add ("Light low end");
    if (boxy > 2.0f)  a.heard.add ("Boxy low mids");
    if (top < -2.5f)  a.heard.add ("Dull top end");
    if (top > 2.5f)   a.heard.add ("Bright top end");
    if (a.crestDb < 9.0f)  a.heard.add ("Dense, limited");
    if (a.crestDb > 16.0f) a.heard.add ("Dynamic");
    if (a.heard.isEmpty()) a.heard.add ("Balanced");

    return a;
}

std::array<float, 6> coefficientsFor (const BandSpec& s, const BandSetting& b, double sr)
{
    using AC = juce::dsp::IIR::ArrayCoefficients<float>;
    const float fc = juce::jlimit (s.fMin, std::min (s.fMax, (float) (0.45 * sr)), b.freq);
    const float g = juce::Decibels::decibelsToGain (b.gainDb);
    switch (s.type)
    {
        case LowShelf:  return AC::makeLowShelf (sr, fc, s.qDefault, g);
        case HighShelf: return AC::makeHighShelf (sr, fc, s.qDefault, g);
        case Peak:      break;
    }
    return AC::makePeakFilter (sr, fc, juce::jlimit (0.3f, 4.0f, b.q), g);
}

float bandResponseDb (const BandSpec& s, const BandSetting& b, float f, double sr)
{
    if (std::abs (b.gainDb) < 0.01f)
        return 0.0f;

    const auto c = coefficientsFor (s, b, sr);
    const double w = juce::MathConstants<double>::twoPi * std::min ((double) f, 0.499 * sr) / sr;
    const std::complex<double> z1 = std::polar (1.0, -w), z2 = z1 * z1;
    const auto num = (double) c[0] + (double) c[1] * z1 + (double) c[2] * z2;
    const auto den = (double) c[3] + (double) c[4] * z1 + (double) c[5] * z2;
    return (float) (20.0 * std::log10 (std::abs (num / den) + 1.0e-12));
}

float eqResponseDb (const EqSetting& eq, float f, double sr)
{
    float sum = 0.0f;
    for (int b = 0; b < numBands; ++b)
        sum += bandResponseDb (bandSpecs()[(size_t) b], eq[(size_t) b], f, sr);
    return sum;
}

float weightedLevelDb (const Analysis& a, const EqSetting& eq, double sr)
{
    double s = 0.0;
    for (int i = 0; i < gridSize; ++i)
        s += a.powerLin[(size_t) i] * std::pow (10.0, eqResponseDb (eq, gridFreq (i), sr) / 10.0);
    return (float) (10.0 * std::log10 (s + 1.0e-20));
}

Suggestion suggest (const Analysis& a, Mode mode, double sr, float strength)
{
    Suggestion out;
    out.eq = flatSetting();
    if (! a.valid)
        return out;

    const auto& specs = bandSpecs();
    const Curve target = targetCurve (mode);
    Curve resid {}, weight {};
    for (int i = 0; i < gridSize; ++i)
    {
        const float f = gridFreq (i);
        weight[(size_t) i] = (f < 30.0f || f > 0.45f * (float) sr) ? 0.0f : (f > 15000.0f ? 0.3f : 1.0f);
        resid[(size_t) i] = juce::jlimit (-6.0f, 6.0f, (target[(size_t) i] - a.spectrumDb[(size_t) i]) * strength);
    }

    // Disjoint search ranges, so neighbouring bands do not fight each other.
    static const float searchLo[numBands] = { 40.0f, 70.0f, 320.0f, 2600.0f, 7000.0f };
    static const float searchHi[numBands] = { 160.0f, 300.0f, 2400.0f, 7000.0f, 14000.0f };
    static const int order[numBands] = { 0, 4, 1, 2, 3 };   // shelves first, then bells
    std::array<Curve, numBands> contrib {};

    for (int pass = 0; pass < 2; ++pass)
    {
        for (int oi = 0; oi < numBands; ++oi)
        {
            const int b = order[oi];
            const auto& spec = specs[(size_t) b];
            for (int i = 0; i < gridSize; ++i)
                resid[(size_t) i] += contrib[(size_t) b][(size_t) i];

            const float limit = spec.type == Peak ? 3.0f : 4.0f;
            std::vector<float> qs = spec.type == Peak ? std::vector<float> { 0.5f, 0.8f, 1.4f }
                                                      : std::vector<float> { spec.qDefault };
            float bestErr = std::numeric_limits<float>::max(), bestG = 0.0f;
            float bestF = spec.fDefault, bestQ = spec.qDefault;

            for (int fk = 0; fk < 24; ++fk)
            {
                const float f = searchLo[b] * std::pow (searchHi[b] / searchLo[b], (float) fk / 23.0f);
                if (f > spec.fMax || f > 0.45f * (float) sr)
                    continue;

                for (float q : qs)
                {
                    Curve shape {};
                    double num = 0.0, den = 0.0;
                    for (int i = 0; i < gridSize; ++i)
                    {
                        shape[(size_t) i] = bandResponseDb (spec, { 3.0f, f, q }, gridFreq (i), sr) / 3.0f;
                        num += weight[(size_t) i] * resid[(size_t) i] * shape[(size_t) i];
                        den += weight[(size_t) i] * shape[(size_t) i] * shape[(size_t) i];
                    }
                    if (den < 1.0e-6)
                        continue;

                    // The extra term in the denominator favours small moves over big ones.
                    const float g = juce::jlimit (-limit, limit, (float) (num / (den * 1.35)));
                    double err = 0.0;
                    for (int i = 0; i < gridSize; ++i)
                    {
                        const double d = resid[(size_t) i] - g * shape[(size_t) i];
                        err += weight[(size_t) i] * d * d;
                    }
                    if (err < bestErr) { bestErr = (float) err; bestG = g; bestF = f; bestQ = q; }
                }
            }

            bestG = std::round (bestG * 2.0f) / 2.0f;   // 0.5 dB steps
            if (std::abs (bestG) < 0.5f)
                out.eq[(size_t) b] = { 0.0f, spec.fDefault, spec.qDefault };
            else
                out.eq[(size_t) b] = { bestG, (float) juce::roundToInt (bestF), bestQ };

            for (int i = 0; i < gridSize; ++i)
            {
                contrib[(size_t) b][(size_t) i] = bandResponseDb (spec, out.eq[(size_t) b], gridFreq (i), sr);
                resid[(size_t) i] -= contrib[(size_t) b][(size_t) i];
            }
        }
    }

    for (int i = 0; i < gridSize; ++i)
        out.eqCurveDb[(size_t) i] = eqResponseDb (out.eq, gridFreq (i), sr);
    return out;
}
} // namespace bat
