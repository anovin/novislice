// Standalone tests for the NoviSlice engine and chopper:
//   cmake --build build --target EngineTest && ./build/EngineTest
#include "../Source/Chopper.h"
#include "../Source/SliceEngine.h"

#include <cmath>
#include <cstdio>
#include <vector>

using namespace novi;

static int failures = 0, checks = 0;
#define CHECK(cond, msg) do { ++checks; if (! (cond)) { ++failures; std::printf ("FAIL: %s (line %d)\n", msg, __LINE__); } } while (0)

constexpr double sr = 48000.0;

// 8 s. Each sixteenth of it (0.5 s) is a DC level of 0.05 * (step + 1).
static SampleData makeStepSample()
{
    SampleData d;
    const int len = (int) (sr * 8.0);
    d.left.resize ((size_t) len);
    d.right.resize ((size_t) len);
    for (int i = 0; i < len; ++i)
        d.left[(size_t) i] = d.right[(size_t) i] = 0.05f * (i * 16 / len + 1);
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

static SampleData makeRamp (double seconds)
{
    SampleData d;
    const int len = (int) (sr * seconds);
    d.left.resize ((size_t) len);
    d.right.resize ((size_t) len);
    for (int i = 0; i < len; ++i)
        d.left[(size_t) i] = d.right[(size_t) i] = (float) i / len;
    return d;
}

// Silence with short decaying noise hits at the given times (seconds).
static SampleData makeHits (const std::vector<double>& times, double seconds)
{
    SampleData d;
    const int len = (int) (sr * seconds);
    d.left.assign ((size_t) len, 0.f);
    uint32_t r = 12345;
    for (double t : times)
    {
        const int a = (int) (t * sr);
        for (int i = 0; i < (int) (0.08 * sr) && a + i < len; ++i)
        {
            r = r * 1664525u + 1013904223u;
            const float noise = ((r >> 8) / 8388608.f) - 1.f;
            d.left[(size_t) (a + i)] = 0.8f * noise * std::exp (-(float) i / (0.015f * (float) sr));
        }
    }
    d.right = d.left;
    return d;
}

static std::vector<float> render (SliceEngine& e, int n, const Transport& t, std::vector<float>* rightOut = nullptr)
{
    std::vector<float> L ((size_t) n), R ((size_t) n);
    for (int pos = 0; pos < n; pos += 512)
        e.process (L.data() + pos, R.data() + pos, std::min (512, n - pos), t);
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

static float peakIn (const std::vector<float>& x, double fromSec, double toSec)
{
    float p = 0.f;
    for (int i = (int) (fromSec * sr); i < (int) (toSec * sr) && i < (int) x.size(); ++i)
        p = std::max (p, std::abs (x[(size_t) i]));
    return p;
}

// An engine with a sample, a chopper and pads, wired together.
struct Rig
{
    SliceEngine e;
    Chopper c;
    SliceMap map;
    PadArray pads {};
    SampleData data;
    EngineParams p;
    ChopSettings chop;
    std::vector<Slice> manual;
    Transport t;

    explicit Rig (SampleData d) : data (std::move (d))
    {
        e.prepare (sr, 512);
        c.analyse (data.left.data(), data.length(), sr);
        p.sync = false;
        p.xfadeMs = 1.f;
        t.bpm = 120.0;
        apply();
    }

    void apply()
    {
        chop.grid = p;
        map = c.compute (chop, manual);
        p.loopTempo = c.tempo (chop);
        e.setSample (&data);
        e.setSlices (&map);
        e.setPads (&pads);
        e.setParams (p);
    }

    std::vector<float> play (int key, double seconds, float vel = 1.f, bool release = true, std::vector<float>* right = nullptr)
    {
        e.noteOn (noteForSliceKey (key), vel);
        auto o = render (e, (int) (sr * seconds), t, right);
        if (release) { e.noteOff (noteForSliceKey (key)); render (e, 4000, t); }
        return o;
    }
};

int main()
{
    // --- key mapping ----------------------------------------------------------------
    CHECK (noteForSliceKey (0) == 60 && noteForSliceKey (1) == 62 && noteForSliceKey (7) == 72 && noteForSliceKey (15) == 86, "white keys from C5");
    CHECK (sliceKeyForNote (61) == -1, "black keys are not slices");
    for (int k = 0; k < maxSlices; ++k)
        CHECK (sliceKeyForNote (noteForSliceKey (k)) == k, "mapping round trip");

    // --- chop: BPM grid ---------------------------------------------------------------
    {
        Rig r (makeStepSample());       // 8 s = 4 bars at 120
        r.p.bars = 4; r.p.slicesPerBar = 4; r.apply();
        CHECK (r.map.count == 16, "auto grid: 4 bars x 4");
        CHECK (std::abs (r.c.tempo (r.chop) - 120.0) < 0.01, "auto tempo from bars and length");
        r.p.triplet = true; r.apply();
        CHECK (r.map.count == 24, "triplet: 4 bars x 6");
        r.p.triplet = false; r.p.bars = 8; r.p.slicesPerBar = 8; r.apply();
        CHECK (r.map.count == 32, "capped at 32");
        r.p.bars = 4; r.p.slicesPerBar = 4; r.p.sampleBpm = 90.f; r.apply();
        CHECK (r.map.count == 12, "90 BPM over 8 s = 3 bars fit");
        r.p.bars = 2; r.apply();
        CHECK (r.map.count == 8, "BARS still picks how many bars with a BPM set");
        CHECK (std::abs ((r.map.s[1].start - r.map.s[0].start) * 8.0 - 240.0 / 90.0 / 4.0) < 1e-4, "slice = a 16th at 90 BPM");
        // trim: second half only, auto tempo over 4 s with 2 bars = 120
        r.p.sampleBpm = 0.f; r.p.bars = 2; r.chop.trimStart = 0.5; r.apply();
        CHECK (r.map.count == 8 && std::abs (r.map.s[0].start - 0.5) < 1e-9, "trim start moves the grid");
        CHECK (std::abs (r.c.tempo (r.chop) - 120.0) < 0.01, "tempo uses the trimmed length");
        r.chop.trimStart = 0.0; r.apply();
    }

    // --- chop: regions / transient / manual / zero snap ------------------------------
    {
        Rig r (makeStepSample());
        r.chop.mode = chopRegions; r.chop.regions = 5; r.apply();
        CHECK (r.map.count == 5 && std::abs (r.map.s[2].start - 0.4) < 1e-9, "5 equal regions");

        std::vector<double> times { 0.1, 0.5, 0.75, 1.3, 1.6, 2.2 };
        Rig h (makeHits (times, 2.5));
        h.chop.mode = chopTransient; h.chop.sensitivity = 0.6f; h.chop.minTimeMs = 50.f; h.apply();
        CHECK (h.map.count == 7, "transient chop: start + 6 hits");
        bool near = true;
        for (size_t i = 0; i < times.size() && i + 1 < (size_t) h.map.count; ++i)
            near = near && std::abs (h.map.s[i + 1].start * 2.5 - times[i]) < 0.006;
        CHECK (near, "transient slices land on the hits (within 6 ms)");
        h.chop.minTimeMs = 400.f; h.apply();
        CHECK (h.map.count < 7, "min time drops hits that are too close");

        r.chop.mode = chopManual;
        r.manual = { { 0.0, 0.25 }, { 0.6, 0.7 }, { 0.25, 0.6 } };
        r.apply();
        CHECK (r.map.count == 3 && std::abs (r.map.s[1].start - 0.25) < 1e-9 && std::abs (r.map.s[2].end - 0.7) < 1e-9, "manual slices sorted");

        Rig z (makeSine (100.0, 1.0));
        z.chop.mode = chopRegions; z.chop.regions = 3; z.chop.zeroSnap = true; z.apply();
        const int at = (int) std::lround (z.map.s[1].start * z.data.length());
        CHECK (std::abs (z.data.left[(size_t) at]) < 0.02f, "zero snap lands on a zero crossing");
    }

    // --- basic playback ---------------------------------------------------------------
    {
        Rig r (makeStepSample());
        auto o = r.play (5, 0.1);
        CHECK (std::abs (o[2400] - 0.30f) < 0.01f, "slice 5 plays its own audio");

        r.e.noteOn (noteForSliceKey (5), 1.f);
        render (r.e, 2400, r.t);
        r.e.noteOn (noteForSliceKey (2), 1.f);
        o = render (r.e, 4800, r.t);
        CHECK (std::abs (o[2400] - 0.15f) < 0.01f, "mono: slice 2 chokes slice 5");
        r.e.noteOff (noteForSliceKey (5)); r.e.noteOff (noteForSliceKey (2));
        o = render (r.e, 4800, r.t);
        CHECK (std::abs (o[4000]) < 1e-4f, "silent after note off");

        o = r.play (0, 0.75, 1.f, false);
        CHECK (std::abs (o[(size_t) (sr * 0.7)]) < 1e-4f, "slice stops at its end");
        r.e.noteOff (noteForSliceKey (0)); render (r.e, 2000, r.t);

        r.p.full = true; r.apply();
        o = r.play (0, 0.75);
        CHECK (std::abs (o[(size_t) (sr * 0.7)] - 0.10f) < 0.01f, "FULL plays into the next slice");
        r.p.full = false;

        r.p.loop = true; r.apply();
        o = r.play (3, 1.3);
        CHECK (std::abs (o[(size_t) (sr * 1.2)] - 0.20f) < 0.01f, "LOOP keeps playing slice 3");
        r.p.loop = false; r.apply();

        std::array<int, maxSlices> order {};
        for (int i = 0; i < maxSlices; ++i) order[(size_t) i] = i;
        std::swap (order[0], order[9]);
        r.e.setOrder (order);
        o = r.play (0, 0.1);
        CHECK (std::abs (o[2400] - 0.50f) < 0.01f, "shuffled key 0 plays slice 9");
        for (int i = 0; i < maxSlices; ++i) order[(size_t) i] = i;
        r.e.setOrder (order);
    }

    // --- sync, key lock, pitch ----------------------------------------------------------
    {
        Rig r (makeSine (440.0, 8.0));    // auto tempo = 120
        r.p.sync = true; r.apply();
        r.t.bpm = 60.0;
        auto o = r.play (0, 1.2);
        CHECK (std::abs (estimateHz (o, 4800, 19200) - 440.0) < 12.0, "sync at half the tempo keeps the tone");
        CHECK (peakIn (o, 0.85, 0.95) > 0.2f && peakIn (o, 1.05, 1.2) < 0.05f, "...and the slice stretches to 1 s");
        r.t.bpm = 180.0;
        o = r.play (0, 0.5);
        CHECK (std::abs (estimateHz (o, 2400, 12000) - 440.0) < 15.0, "sync faster keeps the tone");
        CHECK (peakIn (o, 0.36, 0.45) < 0.05f, "...and the slice shortens to 1/3 s");
        r.t.bpm = 120.0;
        r.p.pitchSemis = 12.f; r.apply();
        o = r.play (0, 0.5);
        CHECK (std::abs (estimateHz (o, 4800, 19200) - 880.0) < 20.0, "+12 at the loop's own tempo");
        r.t.bpm = 60.0;
        o = r.play (0, 0.5);
        CHECK (std::abs (estimateHz (o, 4800, 19200) - 880.0) < 20.0, "+12 while synced slower is still +12");
        r.t.bpm = 120.0;
        r.p.sync = false; r.apply();
        o = r.play (0, 0.7);
        CHECK (std::abs (estimateHz (o, 4800, 19200) - 880.0) < 20.0, "pitch +12 without sync");
        CHECK (peakIn (o, 0.40, 0.45) > 0.2f && peakIn (o, 0.55, 0.7) < 0.05f, "pitch keeps the tempo (slice still 0.5 s)");
        r.p.pitchSemis = 0.f; r.apply();

        // set BPM syncs: loop 60 BPM, host 120 -> slice (1 s at 60) plays in 0.5 s
        Rig b (makeStepSample());
        b.p.sampleBpm = 60.f; b.p.sync = true; b.apply();
        b.t.bpm = 120.0;
        o = b.play (0, 0.7);
        CHECK (peakIn (o, 0.58, 0.68) < 1e-4f, "set Loop BPM syncs to the host tempo");
    }

    // --- roll -------------------------------------------------------------------------
    {
        Rig r (makeSine (440.0, 8.0));
        r.e.noteOn (noteForSliceKey (0), 1.f);
        render (r.e, 4800, r.t);
        r.e.noteOn (firstPerfKey + fxRoll8, 1.f);
        auto o = render (r.e, (int) (sr * 0.8), r.t);
        const int eighth = (int) (sr * 0.25);
        double diff = 0.0;
        for (int i = eighth + 2000; i < eighth + 8000; ++i)
            diff += std::abs (o[(size_t) i] - o[(size_t) (i + eighth)]);
        CHECK (diff / 6000.0 < 0.02, "roll repeats the last eighth");
        CHECK ((r.e.uiPerfMask.load() & (1u << fxRoll8)) != 0, "UI sees the roll");
        r.e.noteOff (firstPerfKey + fxRoll8);
        r.e.noteOff (noteForSliceKey (0));
        render (r.e, 4800, r.t);
    }

    // --- per-pad settings ---------------------------------------------------------------
    {
        Rig r (makeStepSample());
        r.pads[0].playMode = playOneShot;
        r.e.noteOn (noteForSliceKey (0), 1.f);
        render (r.e, 2400, r.t);
        r.e.noteOff (noteForSliceKey (0));
        auto o = render (r.e, 9600, r.t);
        CHECK (std::abs (o[4800] - 0.05f) < 0.01f, "One Shot keeps playing after release");
        render (r.e, 24000, r.t);
        r.pads[0].playMode = playNoteOff;
        r.e.noteOn (noteForSliceKey (0), 1.f);
        o = render (r.e, 4800, r.t);
        CHECK (peakIn (o, 0.0, 0.1) < 1e-4f, "Note Off pad is silent while held");
        r.e.noteOff (noteForSliceKey (0));
        o = render (r.e, 4800, r.t);
        CHECK (std::abs (o[2400] - 0.05f) < 0.01f, "Note Off pad sounds on release");
        render (r.e, 24000, r.t);
        r.pads[0] = PadSettings();

        // poly: two pads overlap; mono cuts
        r.p.poly = true; r.apply();
        r.e.noteOn (noteForSliceKey (1), 1.f);
        r.e.noteOn (noteForSliceKey (3), 1.f);
        o = render (r.e, 4800, r.t);
        CHECK (std::abs (o[2400] - 0.30f) < 0.01f, "poly: slices 1 and 3 sound together (0.10 + 0.20)");
        r.e.noteOff (noteForSliceKey (1)); r.e.noteOff (noteForSliceKey (3)); render (r.e, 4000, r.t);
        // mute group cuts within poly
        r.pads[1].muteGroup = 2; r.pads[3].muteGroup = 2;
        r.e.noteOn (noteForSliceKey (1), 1.f);
        r.e.noteOn (noteForSliceKey (3), 1.f);
        o = render (r.e, 4800, r.t);
        CHECK (std::abs (o[2400] - 0.20f) < 0.01f, "mute group: pad 3 cuts pad 1");
        r.e.noteOff (noteForSliceKey (1)); r.e.noteOff (noteForSliceKey (3)); render (r.e, 4000, r.t);
        r.pads[1] = r.pads[3] = PadSettings();
        r.p.poly = false; r.apply();

        // level and pan
        r.pads[2].levelDb = -6.0206f; r.pads[2].pan = -1.f;
        std::vector<float> right;
        o = r.play (2, 0.1, 1.f, true, &right);
        CHECK (std::abs (o[2400] - 0.15f * 0.5f * 1.41421f) < 0.01f && std::abs (right[2400]) < 1e-4f, "pad level -6 dB, panned hard left");
        r.pads[2] = PadSettings();

        // tail runs into the next slice
        r.pads[0].tailMs = 200.f;
        o = r.play (0, 0.65);
        CHECK (std::abs (o[(size_t) (sr * 0.6)] - 0.10f) < 0.01f, "tail adds audio past the slice end");
        r.pads[0] = PadSettings();

        // loop with repeats
        r.pads[4].loop = true; r.pads[4].repeats = 2;
        o = r.play (4, 1.3);
        CHECK (std::abs (o[(size_t) (sr * 0.75)] - 0.25f) < 0.01f && peakIn (o, 1.05, 1.25) < 1e-4f, "loop 2 repeats then stops");
        r.pads[4] = PadSettings();

        // slice motion: key 0 walks 0,1,2
        r.pads[0].motion = motionNext; r.pads[0].cycles = 3;
        float lv[4];
        for (float& x : lv) { o = r.play (0, 0.05); x = o[1200]; }
        CHECK (std::abs (lv[0] - 0.05f) < 0.01f && std::abs (lv[1] - 0.10f) < 0.01f && std::abs (lv[2] - 0.15f) < 0.01f && std::abs (lv[3] - 0.05f) < 0.01f,
               "slice motion steps through 3 slices and wraps");
        r.pads[0] = PadSettings();

        // mute / solo
        r.pads[6].mute = true;
        o = r.play (6, 0.1);
        CHECK (peakIn (o, 0.0, 0.1) < 1e-4f, "muted pad is silent");
        r.pads[6].mute = false; r.pads[7].solo = true;
        o = r.play (6, 0.1);
        CHECK (peakIn (o, 0.0, 0.1) < 1e-4f, "solo on another pad silences this one");
        o = r.play (7, 0.1);
        CHECK (std::abs (o[2400] - 0.40f) < 0.01f, "soloed pad plays");
        r.pads[7] = PadSettings();

        // full level and velocity
        o = r.play (1, 0.1, 0.25f);
        CHECK (std::abs (o[2400] - 0.10f * 0.25f) < 0.005f, "velocity scales the level");
        r.p.fullLevel = true; r.apply();
        o = r.play (1, 0.1, 0.25f);
        CHECK (std::abs (o[2400] - 0.10f) < 0.005f, "Full Level ignores velocity");
        r.p.fullLevel = false; r.apply();

        // reverse a pad (ramp sample)
        Rig ramp (makeRamp (8.0));
        ramp.pads[0].reverse = true;
        o = ramp.play (0, 0.3);
        CHECK (o[4800] > o[9600] && o[4800] > 0.025f, "reversed pad plays backwards");
    }

    // --- pad tune and filter ------------------------------------------------------------
    {
        Rig r (makeSine (440.0, 8.0));
        r.pads[0].tune = 12.f;
        auto o = r.play (0, 0.45);
        CHECK (std::abs (estimateHz (o, 4800, 19200) - 880.0) < 20.0, "pad tune +12");
        r.pads[0] = PadSettings();
        Rig hi (makeSine (6000.0, 8.0));
        hi.pads[0].cutoffHz = 500.f;
        o = hi.play (0, 0.3);
        CHECK (peakIn (o, 0.1, 0.25) < 0.05f, "pad low-pass at 500 Hz removes a 6 kHz tone");
    }

    // --- note repeat and 16 levels -----------------------------------------------------
    {
        // Ramp sample: every retrigger jumps the output back down to the slice start.
        Rig nr (makeRamp (8.0));
        nr.p.noteRepeat = true; nr.p.repeatRate = 2; nr.apply();
        nr.e.noteOn (noteForSliceKey (0), 1.f);
        auto o = render (nr.e, (int) (sr * 1.0), nr.t);
        nr.e.noteOff (noteForSliceKey (0));
        int resets = 0;
        for (size_t i = 400; i < o.size(); i += 200)
            if (o[i] < o[i - 400] - 0.004f) { ++resets; i += 2000; }
        CHECK (resets >= 7 && resets <= 8, "note repeat 1/16 at 120 BPM: 8 hits a second");

        nr.p.swing = 0.66f; nr.apply();
        nr.e.noteOn (noteForSliceKey (0), 1.f);
        o = render (nr.e, (int) (sr * 1.0), nr.t);
        nr.e.noteOff (noteForSliceKey (0));
        std::vector<int> at;
        for (size_t i = 400; i < o.size(); i += 100)
            if (o[i] < o[i - 400] - 0.004f) { at.push_back ((int) i); i += 2000; }
        bool swung = at.size() >= 4;
        if (swung)
        {
            const int gapA = at[1] - at[0], gapB = at[2] - at[1];
            swung = std::abs (gapA - gapB) > (int) (0.02 * sr);   // long-short pairs
        }
        CHECK (swung, "swing makes uneven steps");

        Rig r (makeStepSample());

        r.p.sixteenLevels = true; r.p.sixteenType = sixteenVelocity; r.p.sixteenSource = 3; r.apply();
        o = r.play (0, 0.1);
        const float low = o[2400];
        o = r.play (15, 0.1);
        CHECK (std::abs (low - 0.20f / 16.f) < 0.003f && std::abs (o[2400] - 0.20f) < 0.005f, "16 levels: pad 1 quietest, pad 16 full, both play the source slice");
        r.p.sixteenLevels = false; r.apply();
    }

    // --- stress: everything at once ---------------------------------------------------
    {
        Rig r (makeSine (440.0, 8.0));
        r.p.textureAmount = 1.f; r.p.grainMs = 20.f; r.p.vinyl = 1.f; r.p.pitchSemis = 24.f; r.p.glideMs = 300.f;
        r.p.cutoffHz = 200.f; r.p.resonance = 1.f; r.p.lfoRate = 5; r.p.lfoDepth = 1.f; r.p.filterMode = 2;
        r.p.bits = 4.f; r.p.downsample = 32; r.p.delayMix = 1.f; r.p.delayFeedback = 0.95f; r.p.delayTime = 0;
        r.p.attackMs = 500.f; r.p.releaseMs = 2000.f; r.p.outGainDb = 12.f; r.p.loop = true; r.p.keyLock = true; r.p.sync = true;
        r.p.poly = true; r.p.noteRepeat = true; r.p.repeatRate = 6;
        for (auto& pd : r.pads) { pd.tune = -24.f; pd.tailMs = 500.f; pd.reverse = true; pd.cutoffHz = 100.f; pd.motion = motionRandom; }
        r.apply();
        for (int fx = 0; fx < numPerfFx; ++fx) r.e.noteOn (firstPerfKey + fx, 1.f);
        for (int k = 0; k < 16; ++k) r.e.noteOn (noteForSliceKey (k), 1.f);
        auto o = render (r.e, (int) (sr * 4), r.t);
        bool finite = true; float peak = 0.f;
        for (float s : o) { finite = finite && std::isfinite (s); peak = std::max (peak, std::abs (s)); }
        CHECK (finite, "stress: output is finite");
        CHECK (peak < 30.f, "stress: output stays bounded");

        r.e.setSample (nullptr);
        render (r.e, 4800, r.t);
        r.e.setSample (&r.data);
        r.e.allNotesOff();
        o = render (r.e, 4800, r.t);
        finite = true;
        for (float s : o) finite = finite && std::isfinite (s);
        CHECK (finite, "sample swap is safe");
    }

    std::printf ("%d/%d checks passed\n", checks - failures, checks);
    return failures == 0 ? 0 : 1;
}
