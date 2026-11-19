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
constexpr int firstSliceKey = 60;  // C5 in FL Studio; slices go up the white keys
constexpr int firstPerfKey  = 36;  // C3 in FL Studio; effects go up chromatically

// MIDI note for slice key index i (white keys from C5), or -1 if out of range.
int noteForSliceKey (int keyIndex);
// Slice key index for a MIDI note, or -1 if the note is not a slice key.
int sliceKeyForNote (int note);

struct EngineParams
{
    int   bars = 4;            // 1, 2, 4, 8
    int   slicesPerBar = 4;    // 1, 2, 4, 8
    bool  triplet = false;     // 2/4/8 per bar become 3/6/12
    bool  full = false;        // keep playing past the slice to the end of the loop
    bool  sync = true;         // stretch the loop to the host tempo
    bool  keyLock = false;     // keep pitch when syncing (granular)
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

    // Audio thread only. The pointer must stay valid until the next call (or nullptr).
    void setSample (const SampleData* data);
    void setParams (const EngineParams& p) { params = p; }
    // keyIndex -> slice index (shuffle). Values outside 0..maxSlices-1 fall back to identity.
    void setOrder (const std::array<int, maxSlices>& order) { sliceOrder = order; }

    void noteOn (int note, float velocity);
    void noteOff (int note);
    void allNotesOff();

    // Renders (replaces) n samples into left/right.
    void process (float* left, float* right, int n, const Transport& t);

    int  numSlices() const;
    double loopBpm() const;       // tempo implied by the loop length and the BARS setting

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
        float  releaseStep = 0.f;
        uint64_t age = 0;
        // granular reader
        double grainAnchor[2] { 0.0, 0.0 };
        double grainAge[2] { 0.0, 0.0 };
    };

    float readSample (const std::vector<float>& buf, double pos) const;
    void  startVoice (int key, int note, float velocity);
    void  renderVoices (float* L, float* R, int n, double speed);
    void  updateRollLength (int fx, double beatSamples);
    float msToSamples (float ms) const { return (float) (ms * 0.001 * sr); }
    float randomUni();   // -1..1

    double sr = 44100.0;
    EngineParams params;
    const SampleData* sample = nullptr;
    std::array<int, maxSlices> sliceOrder {};

    static constexpr int numVoices = 6;
    std::array<Voice, numVoices> voices;
    uint64_t voiceCounter = 0;

    std::array<bool, numPerfFx> perf {};
    std::array<bool, 128> heldNotes {};

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
