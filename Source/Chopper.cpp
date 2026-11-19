#include "Chopper.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace novi
{

void Chopper::clear()
{
    mono.clear();
    onsets.clear();
    len = 0;
}

void Chopper::analyse (const float* data, int length, double sampleRate)
{
    clear();
    if (data == nullptr || length < 64 || sampleRate <= 0)
        return;

    len = length;
    rate = sampleRate;
    mono.assign (data, data + length);

    // Short-time loudness in dB, then a rise detector: how far each frame jumps above the recent past.
    constexpr int hop = 256, win = 1024;
    const int frames = std::max (1, (length - win) / hop + 1);
    std::vector<float> db ((size_t) frames);
    for (int k = 0; k < frames; ++k)
    {
        double sum = 0.0;
        const int a = k * hop, b = std::min (length, a + win);
        for (int i = a; i < b; ++i)
            sum += (double) mono[(size_t) i] * mono[(size_t) i];
        db[(size_t) k] = (float) (10.0 * std::log10 (sum / std::max (1, b - a) + 1e-12));
    }

    std::vector<float> flux ((size_t) frames, 0.f);
    for (int k = 1; k < frames; ++k)
    {
        float past = 0.f;
        int count = 0;
        for (int j = 1; j <= 4 && k - j >= 0; ++j) { past += db[(size_t) (k - j)]; ++count; }
        past /= (float) count;
        flux[(size_t) k] = (db[(size_t) k] > -50.f) ? std::max (0.f, db[(size_t) k] - past) : 0.f;
    }

    for (int k = 1; k < frames; ++k)
    {
        const float f = flux[(size_t) k];
        if (f < 1.f) continue;
        if (f < flux[(size_t) (k - 1)] || (k + 1 < frames && f < flux[(size_t) (k + 1)])) continue;

        // Find where the hit actually starts: the first sample that climbs to 30% of the frame's peak.
        const int a = std::max (0, (k - 1) * hop), b = std::min (length, k * hop + win);
        float peak = 0.f;
        for (int i = k * hop; i < b; ++i) peak = std::max (peak, std::abs (mono[(size_t) i]));
        int pos = k * hop;
        for (int i = a; i < b; ++i)
            if (std::abs (mono[(size_t) i]) >= 0.3f * peak) { pos = i; break; }
        pos = std::max (0, pos - (int) (0.002 * rate));     // a hair before, to keep the attack
        onsets.push_back ({ pos, f });
    }
}

double Chopper::tempo (const ChopSettings& s) const
{
    const double trimLen = std::max (0.0, (s.trimEnd - s.trimStart)) * len;
    return SliceEngine::tempoOf (s.grid, trimLen, rate);
}

double Chopper::snapToZero (double fraction) const
{
    if (len < 2 || mono.empty())
        return fraction;
    const int centre = std::clamp ((int) std::lround (fraction * len), 1, len - 1);
    const int reach = (int) (0.005 * rate);
    for (int d = 0; d <= reach; ++d)
        for (int sgn : { -1, 1 })
        {
            const int i = centre + sgn * d;
            if (i < 1 || i >= len) continue;
            const float a = mono[(size_t) (i - 1)], b = mono[(size_t) i];
            if ((a <= 0.f && b > 0.f) || (a >= 0.f && b < 0.f) || std::abs (b) < 1e-9f)
                return (double) i / len;
        }
    return fraction;
}

SliceMap Chopper::compute (const ChopSettings& s, const std::vector<Slice>& manual) const
{
    SliceMap map;
    if (len < 64)
    {
        map.count = 0;
        return map;
    }

    const double L = len;
    double trimS = std::clamp (s.trimStart, 0.0, 1.0) * L;
    double trimE = std::clamp (s.trimEnd, 0.0, 1.0) * L;
    if (trimE - trimS < 64.0) { trimS = 0.0; trimE = L; }
    const double trimLen = trimE - trimS;

    std::vector<double> starts;
    double regionEnd = trimE;

    if (s.mode == chopManual)
    {
        std::vector<Slice> m;
        for (auto sl : manual)
        {
            double a = std::clamp (sl.start * L, trimS, trimE), b = std::clamp (sl.end * L, trimS, trimE);
            if (b - a >= 32.0) m.push_back ({ a, b });
        }
        std::sort (m.begin(), m.end(), [] (const Slice& x, const Slice& y) { return x.start < y.start; });
        if (m.empty()) m.push_back ({ trimS, trimE });
        if ((int) m.size() > maxSlices) m.resize ((size_t) maxSlices);
        map.count = (int) m.size();
        for (int i = 0; i < map.count; ++i)
        {
            double a = m[(size_t) i].start, b = m[(size_t) i].end;
            if (s.zeroSnap) { a = snapToZero (a / L) * L; b = snapToZero (b / L) * L; }
            map.s[(size_t) i] = { a / L, std::max (a + 32.0, b) / L };
        }
        map.regionEnd = trimE / L;
        map.trimStart = trimS / L;
        map.trimEnd = trimE / L;
        return map;
    }

    if (s.mode == chopGrid)
    {
        const double sliceLen = SliceEngine::sliceLengthOf (s.grid, trimLen, rate);
        const int count = SliceEngine::sliceCount (s.grid, trimLen, rate);
        for (int i = 0; i < count; ++i)
            starts.push_back (trimS + i * sliceLen);
        regionEnd = std::min (trimE, trimS + count * sliceLen);
    }
    else if (s.mode == chopRegions)
    {
        const int n = std::clamp (s.regions, 1, maxSlices);
        for (int i = 0; i < n; ++i)
            starts.push_back (trimS + i * trimLen / n);
    }
    else // transient
    {
        const float threshold = 9.f - 8.f * std::clamp (s.sensitivity, 0.f, 1.f);   // dB of rise: 9 (only big hits) .. 1 (everything)
        const double minGap = std::max (10.0, (double) s.minTimeMs) * 0.001 * rate;

        std::vector<Onset> cand;
        for (auto& o : onsets)
            if (o.strength >= threshold && o.pos > trimS + minGap * 0.5 && o.pos < trimE - minGap * 0.5)
                cand.push_back (o);
        // strongest first, keeping the minimum spacing
        std::sort (cand.begin(), cand.end(), [] (const Onset& a, const Onset& b) { return a.strength > b.strength; });
        starts.push_back (trimS);
        for (auto& o : cand)
        {
            if ((int) starts.size() >= maxSlices) break;
            bool ok = true;
            for (double st : starts)
                if (std::abs (st - o.pos) < minGap) { ok = false; break; }
            if (ok) starts.push_back (o.pos);
        }
        std::sort (starts.begin(), starts.end());
    }

    if (s.zeroSnap)
        for (size_t i = 0; i < starts.size(); ++i)
            starts[i] = snapToZero (starts[i] / L) * L;

    map.count = std::min ((int) starts.size(), maxSlices);
    for (int i = 0; i < map.count; ++i)
    {
        const double a = starts[(size_t) i];
        const double b = (i + 1 < map.count) ? starts[(size_t) (i + 1)] : regionEnd;
        map.s[(size_t) i] = { a / L, std::max (a + 16.0, b) / L };
    }
    map.regionEnd = regionEnd / L;
    map.trimStart = trimS / L;
    map.trimEnd = trimE / L;
    return map;
}

} // namespace novi
