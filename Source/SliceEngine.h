// NoviSlice - slice playback engine. Plain C++ (no JUCE) so it can be unit tested on its own.
#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace novi
{

// A loaded loop, already resampled to the engine's sample rate.
struct SampleData
{
    std::vector<float> left, right;
    int length() const { return (int) left.size(); }
};

// Performance effects, triggered by MIDI notes 36-51 (C3-D#4 in FL Studio) or the PERFORM buttons.
enum PerfFx
{
    fxReverse = 0, fxRoll4, fxRoll8, fxRoll16, fxRoll32, fxRoll8T, fxRoll16T, fxBurst,
    fxTapeStop, fxHalf, fxDouble, fxDive, fxSweep, fxGate, fxCrush, fxThrow,
    numPerfFx
};

const char* perfFxName (int index);

constexpr int maxSlices     = 32;
constexpr float minSampleBpm = 40.f;   // a Loop BPM below this means "auto"
constexpr float maxSampleBpm = 300.f;
constexpr int firstSliceKey = 60;  // C5 in FL Studio; slices go up the white keys
constexpr int firstPerfKey  = 36;  // C3 in FL Studio; effects go up chromatically

// MIDI note for slice key index i (white keys from C5), or -1 if out of range.
int noteForSliceKey (int keyIndex);
// Slice key index for a MIDI note, or -1 if the note is not a slice key.
int sliceKeyForNote (int note);

// One slice, as fractions (0..1) of the sample's length.
struct Slice { double start = 0.0, end = 0.0; };

// Where the slices are. Worked out on the message thread (see Chopper) and handed to the engine.
struct SliceMap
{
    int count = 0;
    double regionEnd = 1.0;               // end of the chopped part (FULL plays up to here)
    double trimStart = 0.0, trimEnd = 1.0; // the part of the sample in use (PLAY plays this)
    std::array<Slice, 32> s {};
};

enum PlayMode { playNoteOn = 0, playOneShot, playNoteOff };
enum SliceMotion { motionOff = 0, motionNext, motionRandom };
enum SixteenType { sixteenVelocity = 0, sixteenTune, sixteenFilter, sixteenDecay };

// MPC-style settings for each pad (slice key 0..31).
struct PadSettings
{
    float tune = 0.f;         // semitones, -24..24 (fractional = fine tune)
    float levelDb = 0.f;      // -48..+6
    float pan = 0.f;          // -1..1
    float cutoffHz = 20000.f; // pad low-pass
    float tailMs = 0.f;       // extra audio past the slice end
    bool  reverse = false;
    bool  mute = false, solo = false;
    bool  loop = false;       // loop while held (Note On mode)
    int   playMode = playNoteOn;
    int   muteGroup = 0;      // 0 = none, 1..8
    int   repeats = 0;        // with loop: 0 = while held, N = play N times
    int   motion = motionOff; // slice motion
    int   cycles = 0;         // slices the motion walks through, 0 = all

    bool isDefault() const
    {
        auto zero = [] (float v) { return v > -1e-6f && v < 1e-6f; };
        return zero (tune) && zero (levelDb) && zero (pan) && cutoffHz >= 20000.f && zero (tailMs) && ! reverse && ! mute
               && ! solo && ! loop && playMode == playNoteOn && muteGroup == 0 && repeats == 0 && motion == motionOff && cycles == 0;
    }
};

using PadArray = std::array<PadSettings, 32>;

struct EngineParams
{
    double loopTempo = 0.0;    // the loop's tempo used by SYNC (worked out by the processor), 0 = unknown
    float sampleBpm = 0.f;     // tempo of the loop as set by the user; below minSampleBpm = auto (from BARS)
    int   bars = 4;            // how many bars to slice (with sampleBpm set: counted at that tempo from the start)
    int   slicesPerBar = 4;    // 1, 2, 4, 8
    bool  triplet = false;     // 2/4/8 per bar become 3/6/12
    bool  full = false;        // keep playing past the slice to the end of the loop
    bool  sync = true;         // stretch the loop to the host tempo
    bool  keyLock = true;      // unused: sync always keeps the pitch (kept for old projects)
    bool  loop = false;        // loop the slice while the key is held

    float textureAmount = 0.f; // 0..1 grain scatter
    float grainMs = 90.f;      // grain size for the granular reader
    float vinyl = 0.f;         // 0..1 crackle and hiss

    float pitchSemis = 0.f;    // -24..24
    float glideMs = 0.f;
    float xfadeMs = 4.f;       // slice edge fades and choke time

    float cutoffHz = 20000.f;
    float resonance = 0.1f;    // 0..1
    int   lfoRate = 0;         // 0 off, 1 = 1/1, 2 = 1/2, 3 = 1/4, 4 = 1/8, 5 = 1/16
    float lfoDepth = 0.f;      // 0..1 (+-3 octaves at full)
    int   filterMode = 0;      // 0 LP, 1 HP, 2 BP

    float bits = 24.f;         // 4..24
    int   downsample = 1;      // 1..32

    float delayMix = 0.f;      // 0..1
    float delayFeedback = 0.35f;
    int   delayTime = 3;       // 0 1/16, 1 1/8, 2 1/8D, 3 1/4, 4 1/4D, 5 1/2

    float attackMs = 0.f;
    float releaseMs = 0.f;
    float outGainDb = 0.f;

    // MPC-style play modes
    bool  poly = false;        // pads overlap (mute groups still cut) instead of cutting each other off
    bool  fullLevel = false;   // ignore velocity
    float velSens = 1.f;       // 0..1, how much velocity changes the level
    bool  noteRepeat = false;
    int   repeatRate = 2;      // 0 1/4, 1 1/8, 2 1/16, 3 1/32, 4 1/8T, 5 1/16T, 6 1/32T
    float swing = 0.5f;        // 0.5..0.75 (MPC 50%..75%)
    bool  sixteenLevels = false;
    int   sixteenType = sixteenVelocity;
    int   sixteenSource = 0;   // the pad spread over the 16 pads
};

struct Transport
{
    double bpm = 120.0;
    bool   playing = false;
    double ppq = 0.0;          // host position in quarter notes at the start of the block
    bool   hasPpq = false;
};

class SliceEngine
{
public:
    SliceEngine();

    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    // Audio thread only. The pointers must stay valid until the next call (or nullptr).
    void setSample (const SampleData* data);
    void setSlices (const SliceMap* map) { slices = map; }
    void setPads (const PadArray* p) { pads = p; }
    void setParams (const EngineParams& p) { params = p; }
    // keyIndex -> slice index (shuffle). Values outside 0..maxSlices-1 fall back to identity.
    void setOrder (const std::array<int, maxSlices>& order) { sliceOrder = order; }

    void noteOn (int note, float velocity);
    void noteOff (int note);
    void allNotesOff();

    // PLAY button: the whole (trimmed) sample from start to finish, through Sync, Pitch and the effects.
    void startPreview();
    void stopPreview();
    std::atomic<bool> uiPreviewing { false };

    // Renders (replaces) n samples into left/right.
    void process (float* left, float* right, int n, const Transport& t);

    int  numSlices() const { return slices != nullptr ? slices->count : 0; }

    // Grid maths shared with the UI. lengthSamples/sampleRate describe the loop.
    static int    sliceCount (const EngineParams& p, double lengthSamples, double sampleRate);
    static double tempoOf (const EngineParams& p, double lengthSamples, double sampleRate);
    static double sliceLengthOf (const EngineParams& p, double lengthSamples, double sampleRate);
    static int    slicesPerBar (const EngineParams& p);

    // For the UI
    std::atomic<int>   uiActiveSlice { -1 };   // slice index sounding now
    std::atomic<int>   uiActiveKey   { -1 };   // slice key index sounding now
    std::atomic<float> uiPlayhead    { -1.f }; // 0..1 across the loop, -1 if idle
    std::atomic<uint32_t> uiPerfMask { 0 };

private:
    struct Voice
    {
        bool   active = false;
        int    key = -1, note = -1, slice = 0;
        double head = 0.0;           // read position in the sample
        double regionStart = 0.0, regionEnd = 0.0;
        float  velocity = 1.f;
        float  env = 0.f;
        bool   releasing = false;
        bool   granular = false;     // reading through the grain engine (pitch, key lock, texture)
        float  releaseStep = 0.f;
        int    uiKey = -1;           // pad shown lit (differs from key in 16 Levels)
        bool   preview = false;      // started by the PLAY button
        bool   reverse = false;      // pad reverse
        bool   oneShot = false;      // ignores note off
        bool   loop = false;
        int    repeats = 0, plays = 0;
        int    muteGroup = 0;
        double tuneRatio = 1.0;
        float  gainL = 1.f, gainR = 1.f;
        float  decayStep = 0.f, decayGain = 1.f;
        bool   filtered = false;
        float  fa1 = 0.f, fa2 = 0.f, fa3 = 0.f, fk = 2.f;
        float  f1[2] { 0.f, 0.f }, f2[2] { 0.f, 0.f };
        uint64_t age = 0;
        // granular reader
        double grainAnchor[2] { 0.0, 0.0 };
        double grainAge[2] { 0.0, 0.0 };
    };

    float readSample (const std::vector<float>& buf, double pos) const;
    void  trigger (int key, int note, float velocity, int level, bool fromRepeat);
    void  startVoice (int key, int note, float velocity, int level, bool forceOneShot);
    const PadSettings& pad (int key) const;
    void  updateRollLength (int fx, double beatSamples);
    float msToSamples (float ms) const { return (float) (ms * 0.001 * sr); }
    float randomUni();   // -1..1

    double sr = 44100.0;
    EngineParams params;
    const SampleData* sample = nullptr;
    const SliceMap* slices = nullptr;
    const PadArray* pads = nullptr;
    std::array<int, maxSlices> sliceOrder {};
    std::array<int, maxSlices> motionCounter {};

    // what each held note plays (for Note Repeat and Note Off pads)
    struct Held { bool on = false, pendingNoteOff = false; int key = -1, level = -1; float velocity = 1.f; double lastTrigger = -1e9; };
    std::array<Held, 128> held {};
    double sampleClock = 0.0;                     // samples since prepare
    double repeatPos = 0.0;                       // free-running note-repeat position (beats)
    long long nextTick = 0;
    bool repeatSynced = false;

    static constexpr int numVoices = 16;
    std::array<Voice, numVoices> voices;
    uint64_t voiceCounter = 0;

    std::array<bool, numPerfFx> perf {};

    // pitch glide
    float currentSemis = 0.f;
    bool  semisInit = false;

    // tape stop / dive ramps (0..1 progress)
    double tapeStopProgress = 0.0, diveProgress = 0.0, sweepProgress = 0.0;
    double tapeRate = 1.0;

    // beat repeat (rolls / burst)
    std::vector<float> rollL, rollR;
    int  rollActiveFx = -1;
    int  rollLen = 0, rollWrite = 0, rollRead = 0;
    bool rollFilled = false;
    int  burstStage = 0, burstRepeats = 0;
    float rollMix = 0.f;

    // gate
    double gatePhase = 0.0;
    float  gateGain = 1.f;

    // filter (TPT state variable)
    float ic1[2] { 0.f, 0.f }, ic2[2] { 0.f, 0.f };
    double lfoPhase = 0.0;
    float  smoothCutoff = 20000.f;

    // crusher
    int   holdCounter = 0;
    float heldL = 0.f, heldR = 0.f;

    // delay
    std::vector<float> delayL, delayR;
    int   delayWrite = 0;
    float smoothDelay = 0.f;
    float throwWet = 0.f;
    float fbLowL = 0.f, fbLowR = 0.f;

    // vinyl
    float activity = 0.f;
    float hissLp = 0.f, crackleEnv = 0.f, crackleSign = 1.f;
    uint32_t rng = 0x9e3779b9u;

    float outGain = 1.f;
};

} // namespace novi
