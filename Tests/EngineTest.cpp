// Standalone tests for the NoviSlice engine: cmake --build build --target EngineTest && ./build/EngineTest
#include "../Source/SliceEngine.h"

#include <cmath>
#include <cstdio>
#include <vector>

using namespace novi;

static int failures = 0, checks = 0;
#define CHECK(cond, msg) do { ++checks; if (! (cond)) { ++failures; std::printf ("FAIL: %s (line %d)\n", msg, __LINE__); } } while (0)

constexpr double sr = 48000.0;

// 4 bars at 120 BPM = 8 seconds. Each of 16 slices is a DC level of 0.05 * (slice + 1).
static SampleData makeStepSample()
{
    SampleData d;
    const int len = (int) (sr * 8.0);
    d.left.resize ((size_t) len);
    d.right.resize ((size_t) len);
    for (int i = 0; i < len; ++i)
    {
        const int slice = i * 16 / len;
        d.left[(size_t) i] = d.right[(size_t) i] = 0.05f * (slice + 1);
    }
    return d;
}

static SampleData makeSine (double hz, double seconds)
{
    SampleData d;
    const int len = (int) (sr * seconds);
    d.left.resize ((size_t) len);
    d.right.resize ((size_t) len);
    for (int i = 0; i < len; ++i)
        d.left[(size_t) i] = d.right[(size_t) i] = (float) (0.5 * std::sin (6.283185307 * hz * i / sr));
    return d;
}

static std::vector<float> render (SliceEngine& e, int n, const Transport& t, std::vector<float>* rightOut = nullptr)
{
    std::vector<float> L ((size_t) n), R ((size_t) n);
    for (int pos = 0; pos < n; pos += 512)
    {
        const int len = std::min (512, n - pos);
        e.process (L.data() + pos, R.data() + pos, len, t);
    }
    if (rightOut) *rightOut = R;
    return L;
}

static double estimateHz (const std::vector<float>& x, int from, int to)
{
    int crossings = 0;
    for (int i = from + 1; i < to; ++i)
        if (x[(size_t) i - 1] <= 0.f && x[(size_t) i] > 0.f)
            ++crossings;
    return crossings / ((to - from) / sr);
}

int main()
{
    // Key mapping: white keys from C5 (60)
    CHECK (noteForSliceKey (0) == 60, "key 0 is C5");
    CHECK (noteForSliceKey (1) == 62, "key 1 is D5");
    CHECK (noteForSliceKey (7) == 72, "key 7 is C6");
    CHECK (noteForSliceKey (15) == 86, "key 15 is D7");
    CHECK (sliceKeyForNote (61) == -1, "black keys are not slices");
    for (int k = 0; k < maxSlices; ++k)
        CHECK (sliceKeyForNote (noteForSliceKey (k)) == k, "mapping round trip");

    SliceEngine e;
    e.prepare (sr, 512);
    EngineParams p;
    p.bars = 4; p.slicesPerBar = 4; p.sync = false;
    e.setParams (p);
    CHECK (e.numSlices() == 16, "4 bars x 4 = 16 slices");
    p.triplet = true; e.setParams (p);
    CHECK (e.numSlices() == 24, "triplet 4 bars x 6 = 24 slices");
    p.bars = 8; p.slicesPerBar = 8; p.triplet = false; e.setParams (p);
    CHECK (e.numSlices() == 32, "slices capped at 32");
    p.bars = 4; p.slicesPerBar = 4; e.setParams (p);

    auto step = makeStepSample();
    e.setSample (&step);
    CHECK (std::abs (e.loopBpm() - 120.0) < 0.01, "loop BPM from bars and length");

    Transport t;
    t.bpm = 120.0;

    // Slice 5 (key 5 = A5 = 69) should play level 0.30
    e.noteOn (noteForSliceKey (5), 1.f);
    auto out = render (e, 4800, t);
    CHECK (std::abs (out[2400] - 0.30f) < 0.01f, "slice 5 plays its own audio");
    CHECK (e.uiActiveSlice.load() == 5, "UI sees slice 5");

    // Choke: a new slice replaces the old one
    e.noteOn (noteForSliceKey (2), 1.f);
    out = render (e, 4800, t);
    CHECK (std::abs (out[2400] - 0.15f) < 0.01f, "slice 2 chokes slice 5");

    // Note off releases
    e.noteOff (noteForSliceKey (2));
    out = render (e, 4800, t);
    CHECK (std::abs (out[4000]) < 1e-4f, "silent after note off");

    // One-shot stops at slice end (slice is 0.5 s); FULL keeps going
    e.noteOn (noteForSliceKey (0), 1.f);
    out = render (e, (int) (sr * 0.75), t);
    CHECK (std::abs (out[(size_t) (sr * 0.7)]) < 1e-4f, "slice stops at its end");
    e.noteOff (noteForSliceKey (0));
    render (e, 2000, t);

    p.full = true; e.setParams (p);
    e.noteOn (noteForSliceKey (0), 1.f);
    out = render (e, (int) (sr * 0.75), t);
    CHECK (std::abs (out[(size_t) (sr * 0.7)] - 0.10f) < 0.01f, "FULL plays into the next slice");
    e.noteOff (noteForSliceKey (0));
    render (e, 2000, t);
    p.full = false;

    // LOOP repeats the slice while held
    p.loop = true; e.setParams (p);
    e.noteOn (noteForSliceKey (3), 1.f);
    out = render (e, (int) (sr * 1.3), t);
    CHECK (std::abs (out[(size_t) (sr * 1.2)] - 0.20f) < 0.01f, "LOOP keeps playing slice 3");
    e.noteOff (noteForSliceKey (3));
    render (e, 2000, t);
    p.loop = false;

    // Shuffle order: key 0 plays slice 9
    std::array<int, maxSlices> order {};
    for (int i = 0; i < maxSlices; ++i) order[(size_t) i] = i;
    std::swap (order[0], order[9]);
    e.setOrder (order);
    e.setParams (p);
    e.noteOn (noteForSliceKey (0), 1.f);
    out = render (e, 4800, t);
    CHECK (std::abs (out[2400] - 0.50f) < 0.01f, "shuffled key 0 plays slice 9");
    e.noteOff (noteForSliceKey (0));
    render (e, 2000, t);
    for (int i = 0; i < maxSlices; ++i) order[(size_t) i] = i;
    e.setOrder (order);

    // Sync: at 60 BPM host the 120 BPM loop plays at half speed, so slice 0 lasts 1 s
    p.sync = true; e.setParams (p);
    t.bpm = 60.0;
    e.noteOn (noteForSliceKey (0), 1.f);
    out = render (e, (int) (sr * 0.9), t);
    CHECK (std::abs (out[(size_t) (sr * 0.8)] - 0.05f) < 0.01f, "sync stretches the slice");
    e.noteOff (noteForSliceKey (0));
    render (e, 2000, t);

    // Pitch: a 440 Hz loop, slowed by sync to half speed, sounds at 220 Hz without key lock...
    auto sine = makeSine (440.0, 8.0);
    e.setSample (&sine);
    e.noteOn (noteForSliceKey (0), 1.f);
    out = render (e, (int) (sr * 0.5), t);
    double hz = estimateHz (out, 4800, 19200);
    CHECK (std::abs (hz - 220.0) < 8.0, "no key lock: half speed = octave down");
    e.noteOff (noteForSliceKey (0));
    render (e, 4000, t);

    // ...and at 440 Hz with key lock
    p.keyLock = true; e.setParams (p);
    e.noteOn (noteForSliceKey (0), 1.f);
    out = render (e, (int) (sr * 0.5), t);
    hz = estimateHz (out, 4800, 19200);
    CHECK (std::abs (hz - 440.0) < 12.0, "key lock keeps pitch");
    e.noteOff (noteForSliceKey (0));
    render (e, 4000, t);

    // Pitch +12 with key lock at normal tempo = 880 Hz
    t.bpm = 120.0;
    p.pitchSemis = 12.f; e.setParams (p);
    e.noteOn (noteForSliceKey (0), 1.f);
    out = render (e, (int) (sr * 0.5), t);
    hz = estimateHz (out, 4800, 19200);
    CHECK (std::abs (hz - 880.0) < 20.0, "+12 semitones with key lock");
    e.noteOff (noteForSliceKey (0));
    render (e, 4000, t);
    p.pitchSemis = 0.f; p.keyLock = false; p.sync = false;
    e.setParams (p);

    // Roll 1/8: after one eighth (0.25 s at 120) the output repeats
    e.setSample (&sine);
    e.noteOn (noteForSliceKey (0), 1.f);
    render (e, 4800, t);
    e.noteOn (firstPerfKey + fxRoll8, 1.f);
    out = render (e, (int) (sr * 0.8), t);
    const int eighth = (int) (sr * 0.25);
    double diff = 0.0;
    for (int i = eighth + 2000; i < eighth + 8000; ++i)
        diff += std::abs (out[(size_t) i] - out[(size_t) (i + eighth)]);
    CHECK (diff / 6000.0 < 0.02, "roll repeats the last eighth");
    CHECK ((e.uiPerfMask.load() & (1u << fxRoll8)) != 0, "UI sees the roll");
    e.noteOff (firstPerfKey + fxRoll8);
    e.noteOff (noteForSliceKey (0));
    render (e, 4800, t);

    // Every effect at once, every knob at an extreme: no NaN, no blow-up
    p.textureAmount = 1.f; p.grainMs = 20.f; p.vinyl = 1.f; p.pitchSemis = 24.f; p.glideMs = 300.f;
    p.cutoffHz = 200.f; p.resonance = 1.f; p.lfoRate = 5; p.lfoDepth = 1.f; p.filterMode = 2;
    p.bits = 4.f; p.downsample = 32; p.delayMix = 1.f; p.delayFeedback = 0.95f; p.delayTime = 0;
    p.attackMs = 500.f; p.releaseMs = 2000.f; p.outGainDb = 12.f; p.loop = true; p.keyLock = true; p.sync = true;
    e.setParams (p);
    for (int fx = 0; fx < numPerfFx; ++fx) e.noteOn (firstPerfKey + fx, 1.f);
    for (int k = 0; k < 16; ++k) e.noteOn (noteForSliceKey (k), 1.f);
    out = render (e, (int) (sr * 6), t);
    bool finite = true; float peak = 0.f;
    for (float s : out) { finite = finite && std::isfinite (s); peak = std::max (peak, std::abs (s)); }
    CHECK (finite, "stress: output is finite");
    CHECK (peak < 20.f, "stress: output stays bounded");

    // Sample swap mid-play is safe
    e.setSample (nullptr);
    out = render (e, 4800, t);
    e.setSample (&step);
    e.allNotesOff();
    out = render (e, 4800, t);
    finite = true;
    for (float s : out) finite = finite && std::isfinite (s);
    CHECK (finite, "sample swap is safe");

    std::printf ("%d/%d checks passed\n", checks - failures, checks);
    return failures == 0 ? 0 : 1;
}
