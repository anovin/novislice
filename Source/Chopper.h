// NoviSlice - works out where the slices go (MPC-style chop modes). Plain C++, no JUCE.
#pragma once

#include "SliceEngine.h"

#include <vector>

namespace novi
{

enum ChopMode { chopGrid = 0, chopRegions, chopTransient, chopManual, numChopModes };

struct ChopSettings
{
    int    mode = chopGrid;
    EngineParams grid;          // sampleBpm, bars, slicesPerBar, triplet (for the BPM grid and the loop tempo)
    int    regions = 16;        // equal regions
    float  sensitivity = 0.5f;  // transient chop, 0..1 (higher = more slices)
    float  minTimeMs = 60.f;    // transient chop: shortest slice
    double trimStart = 0.0;     // fractions of the sample
    double trimEnd = 1.0;
    bool   zeroSnap = false;    // move slice points to the nearest zero crossing
};

class Chopper
{
public:
    // Analyse a (mono) sample once when it loads; compute() is then cheap.
    void analyse (const float* mono, int length, double sampleRate);
    void clear();

    SliceMap compute (const ChopSettings& s, const std::vector<Slice>& manual) const;

    // The loop's tempo for the trimmed part (Loop BPM if set, otherwise from BARS).
    double tempo (const ChopSettings& s) const;

    // Nearest zero crossing (within a few ms) to a fraction of the sample.
    double snapToZero (double fraction) const;

    int length() const { return len; }
    double sampleRate() const { return rate; }

private:
    struct Onset { int pos; float strength; };

    std::vector<float> mono;
    std::vector<Onset> onsets;
    int len = 0;
    double rate = 44100.0;
};

} // namespace novi
