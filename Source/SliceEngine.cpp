#include "SliceEngine.h"

#include "signalsmith-stretch/signalsmith-stretch.h"

#include <algorithm>
#include <cmath>

namespace novi
{

namespace
{
    constexpr double twoPi = 6.283185307179586;
    constexpr int whiteOffsets[7] = { 0, 2, 4, 5, 7, 9, 11 };

    double frac (double x) { return x - std::floor (x); }
}

const char* perfFxName (int index)
{
    static const char* names[numPerfFx] = {
        "REVERSE", "ROLL 1/4", "ROLL 1/8", "ROLL 1/16", "ROLL 1/32", "ROLL 1/8T", "ROLL 1/16T", "BURST",
        "TAPE STOP", "HALF", "DOUBLE", "DIVE", "SWEEP", "GATE", "CRUSH", "THROW"
    };
    return (index >= 0 && index < numPerfFx) ? names[index] : "";
}

int noteForSliceKey (int keyIndex)
{
    if (keyIndex < 0 || keyIndex >= maxSlices)
        return -1;
    return firstSliceKey + 12 * (keyIndex / 7) + whiteOffsets[keyIndex % 7];
}

int sliceKeyForNote (int note)
{
    if (note < firstSliceKey)
        return -1;
    const int rel = note - firstSliceKey;
    const int octave = rel / 12, semis = rel % 12;
    for (int i = 0; i < 7; ++i)
        if (whiteOffsets[i] == semis)
        {
            const int key = octave * 7 + i;
            return key < maxSlices ? key : -1;
        }
    return -1;
}

SliceEngine::~SliceEngine() = default;

SliceEngine::SliceEngine()
{
    for (int i = 0; i < maxSlices; ++i)
        sliceOrder[(size_t) i] = i;
}

void SliceEngine::prepare (double sampleRate, int)
{
    sr = sampleRate > 0 ? sampleRate : 44100.0;
    const auto rollSize = (size_t) (sr * 4.0);
    rollL.assign (rollSize, 0.f);
    rollR.assign (rollSize, 0.f);
    const auto delaySize = (size_t) (sr * 4.5);
    delayL.assign (delaySize, 0.f);
    delayR.assign (delaySize, 0.f);

    stretchers.clear();
    stretchers.resize (numStretchers);
    const auto inSize = (size_t) (sr * 0.12 * 6.0) + tonalBlock * 8;
    for (auto& st : stretchers)
    {
        st.stretch = std::make_unique<Stretcher>();
        st.stretch->presetDefault (2, (float) sr);
        st.inL.assign (inSize, 0.f);
        st.inR.assign (inSize, 0.f);
        st.outL.assign ((size_t) tonalBlock, 0.f);
        st.outR.assign ((size_t) tonalBlock, 0.f);
    }
    reset();
}

void SliceEngine::reset()
{
    for (auto& v : voices)
        v = Voice();
    for (auto& st : stretchers)
        st.used = false;
    perf.fill (false);
    for (auto& h : held) h = Held();
    motionCounter.fill (0);
    repeatPos = 0.0;
    nextTick = 0;
    semisInit = false;
    tapeStopProgress = diveProgress = sweepProgress = 0.0;
    tapeRate = 1.0;
    rollActiveFx = -1;
    rollFilled = false;
    rollLen = rollWrite = rollRead = 0;
    rollMix = 0.f;
    gatePhase = 0.0;
    gateGain = 1.f;
    ic1[0] = ic1[1] = ic2[0] = ic2[1] = 0.f;
    lfoPhase = 0.0;
    smoothCutoff = 20000.f;
    holdCounter = 0;
    heldL = heldR = 0.f;
    std::fill (delayL.begin(), delayL.end(), 0.f);
    std::fill (delayR.begin(), delayR.end(), 0.f);
    delayWrite = 0;
    smoothDelay = 0.f;
    throwWet = 0.f;
    fbLowL = fbLowR = 0.f;
    activity = 0.f;
    hissLp = crackleEnv = 0.f;
    uiActiveSlice = -1;
    uiActiveKey = -1;
    uiPlayhead = -1.f;
    uiPerfMask = 0;
}

void SliceEngine::setSample (const SampleData* data)
{
    if (data != sample)
    {
        for (auto& v : voices)
            freeVoice (v);
        sample = data;
    }
}

int SliceEngine::slicesPerBar (const EngineParams& p)
{
    int perBar = std::max (1, p.slicesPerBar);
    if (p.triplet && perBar >= 2)
        perBar = perBar * 3 / 2;
    return perBar;
}

double SliceEngine::tempoOf (const EngineParams& p, double lengthSamples, double sampleRate)
{
    if (p.sampleBpm >= minSampleBpm)
        return std::min ((double) p.sampleBpm, (double) maxSampleBpm);
    if (lengthSamples < 2 || sampleRate <= 0)
        return 0.0;
    return std::max (1, p.bars) * 4.0 * 60.0 / (lengthSamples / sampleRate);
}

double SliceEngine::sliceLengthOf (const EngineParams& p, double lengthSamples, double sampleRate)
{
    if (lengthSamples < 2)
        return 1.0;
    if (p.sampleBpm >= minSampleBpm)
    {
        // The grid follows the tempo: one bar = 4 beats at the loop's BPM.
        const double bar = 240.0 / tempoOf (p, lengthSamples, sampleRate) * sampleRate;
        return std::max (16.0, bar / slicesPerBar (p));
    }
    return lengthSamples / sliceCount (p, lengthSamples, sampleRate);
}

int SliceEngine::sliceCount (const EngineParams& p, double lengthSamples, double sampleRate)
{
    const int wanted = std::clamp (std::max (1, p.bars) * slicesPerBar (p), 1, maxSlices);
    if (p.sampleBpm >= minSampleBpm && lengthSamples >= 2)
    {
        // BARS bars from the start of the sample, but only the slices the sample actually reaches
        // (a sliver of tail shorter than 10% of a slice is ignored).
        const double len = sliceLengthOf (p, lengthSamples, sampleRate);
        const int fit = std::max (1, (int) std::ceil (lengthSamples / len - 0.1));
        return std::min (wanted, fit);
    }
    return wanted;
}

const PadSettings& SliceEngine::pad (int key) const
{
    static const PadSettings defaults;
    return (pads != nullptr && key >= 0 && key < maxSlices) ? (*pads)[(size_t) key] : defaults;
}

float SliceEngine::randomUni()
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return (float) ((double) rng / 4294967295.0) * 2.f - 1.f;
}

float SliceEngine::readSample (const std::vector<float>& buf, double pos) const
{
    const int len = (int) buf.size();
    if (pos < 0.0 || pos > (double) (len - 1))
        return 0.f;
    const int i = (int) pos;
    const float t = (float) (pos - i);
    auto at = [&] (int k) { return (k >= 0 && k < len) ? buf[(size_t) k] : 0.f; };
    const float y0 = at (i - 1), y1 = at (i), y2 = at (i + 1), y3 = at (i + 2);
    // 4-point Hermite
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * t + c2) * t + c1) * t + y1;
}

void SliceEngine::noteOn (int note, float velocity)
{
    if (note < 0 || note > 127)
        return;

    const int fx = note - firstPerfKey;
    if (fx >= 0 && fx < numPerfFx)
    {
        if (! perf[(size_t) fx])
        {
            if (fx == fxTapeStop) tapeStopProgress = 0.0;
            if (fx == fxDive)     diveProgress = 0.0;
            if (fx == fxSweep)    sweepProgress = 0.0;
        }
        perf[(size_t) fx] = true;
        return;
    }

    int key = sliceKeyForNote (note);
    if (key < 0)
        return;

    // 16 Levels: the first 16 pads all play one pad, at 16 rising levels.
    int level = -1;
    if (params.sixteenLevels && key < 16)
    {
        level = key;
        key = std::clamp (params.sixteenSource, 0, maxSlices - 1);
    }

    auto& h = held[(size_t) note];
    h.on = true;
    h.key = key;
    h.level = level;
    h.velocity = velocity;
    h.pendingNoteOff = false;
    h.lastTrigger = sampleClock;

    if (pad (key).playMode == playNoteOff && level < 0)
    {
        h.pendingNoteOff = true;        // sounds when the pad is released
        return;
    }
    trigger (key, note, velocity, level, false);
}

void SliceEngine::noteOff (int note)
{
    if (note < 0 || note > 127)
        return;

    const int fx = note - firstPerfKey;
    if (fx >= 0 && fx < numPerfFx)
    {
        perf[(size_t) fx] = false;
        return;
    }

    auto& h = held[(size_t) note];
    const bool wasPending = h.on && h.pendingNoteOff;
    h.on = false;
    h.pendingNoteOff = false;
    if (wasPending)
    {
        startVoice (h.key, note, h.velocity, h.level, true);
        return;
    }

    const float relSamples = std::max (msToSamples (5.f), msToSamples (params.releaseMs));
    for (auto& v : voices)
        if (v.active && ! v.releasing && v.note == note && ! v.oneShot)
        {
            v.releasing = true;
            v.releaseStep = std::max (v.env, 0.001f) / relSamples;
        }
}

void SliceEngine::allNotesOff()
{
    for (int n = 0; n < 128; ++n)
        if (held[(size_t) n].on)
            noteOff (n);
    perf.fill (false);
}

void SliceEngine::startPreview()
{
    if (sample == nullptr || sample->length() < 16)
        return;
    const float chokeSamples = std::max (msToSamples (2.f), msToSamples (params.xfadeMs));
    for (auto& v : voices)
        if (v.active && ! v.releasing)
        {
            v.releasing = true;
            v.releaseStep = std::max (v.env, 0.001f) / chokeSamples;
        }

    Voice* target = &voices[0];
    for (auto& v : voices)
        if (! v.active) { target = &v; break; }
    if (target->slot >= 0) stretchers[(size_t) target->slot].used = false;

    const double len = sample->length();
    const double a = slices != nullptr ? slices->trimStart : 0.0;
    const double b = slices != nullptr ? slices->trimEnd : 1.0;

    Voice& v = *target;
    v = Voice();
    v.active = true;
    v.preview = true;
    v.key = -1;
    v.uiKey = -1;
    v.note = -2;            // never released by a note off
    v.slice = -1;
    v.oneShot = true;
    v.velocity = 1.f;
    v.regionStart = std::clamp (a * len, 0.0, len - 16.0);
    v.regionEnd = std::clamp (b * len, v.regionStart + 16.0, len);
    v.head = perf[fxReverse] ? v.regionEnd - 1.0 : v.regionStart;
    v.env = 1.f;
    v.age = ++voiceCounter;
    const double grain = std::max (256.0, (double) msToSamples (params.grainMs));
    v.grainAnchor[0] = v.grainAnchor[1] = v.head;
    v.grainAge[0] = 0.0;
    v.grainAge[1] = grain * 0.5;
    v.multiSeg = true;
    setupVoiceMode (v);
    uiPreviewing = true;
}

// ---------------------------------------------------------------------------------------------
// Stretch modes

void SliceEngine::freeVoice (Voice& v)
{
    if (v.slot >= 0 && v.slot < (int) stretchers.size())
        stretchers[(size_t) v.slot].used = false;
    v.slot = -1;
    v.active = false;
}

void SliceEngine::setupVoiceMode (Voice& v)
{
    const double dir = (v.reverse != perf[fxReverse]) ? -1.0 : 1.0;
    v.display = v.head;

    if (params.textureAmount > 0.001f)
    {
        v.mode = modeTexture;
        return;
    }

    if (params.stretchMode == stretchTonal)
        for (int i = 0; i < (int) stretchers.size(); ++i)
        {
            auto& slot = stretchers[(size_t) i];
            if (slot.used) continue;
            slot.used = true;
            v.mode = modeTonal;
            v.slot = i;
            auto& st = *slot.stretch;
            const double rate = std::clamp (lastSpeed * lastModRate, 0.05, 4.0);
            st.setTransposeFactor (1.f);
            // Prime the stretcher with the start of the sound, so the output starts right away (no latency).
            const int seekLen = std::min ((int) slot.inL.size(), st.outputSeekLength ((float) rate));
            const bool loop = v.preview ? params.loop : v.loop;
            for (int k = 0; k < seekLen; ++k)
            {
                const double pos = v.head + k * dir;
                slot.inL[(size_t) k] = readTonalInput (v, sample->left, pos, loop);
                slot.inR[(size_t) k] = readTonalInput (v, sample->right, pos, loop);
            }
            float* in[2] = { slot.inL.data(), slot.inR.data() };
            st.outputSeek (in, seekLen);
            v.head += seekLen * dir;
            v.fifoPos = tonalBlock;
            v.inputCarry = 0.0;
            return;
        }

    // BEATS (also the fallback if every stretcher is busy). It borrows a stretcher, if one is free,
    // to fill each slice's slot after the hit.
    v.mode = modeBeats;
    v.slot = -1;
    for (int i = 0; i < (int) stretchers.size(); ++i)
        if (! stretchers[(size_t) i].used)
        {
            stretchers[(size_t) i].used = true;
            v.slot = i;
            break;
        }
    v.timeline = dir > 0 ? v.regionStart : v.regionEnd - 1e-6;
    v.seg = -1;
    v.lastSegGain = 0.f;
    VoiceContext c;
    c.fadeLen = std::max (16.f, msToSamples (params.xfadeMs));
    jumpToSegment (v, dir, c, false);
    v.oldGain = 0.f;
}

int SliceEngine::segmentAt (const Voice& v, double pos, double& a, double& b) const
{
    if (pos < v.regionStart || pos >= v.regionEnd)
        return -1;
    if (! v.multiSeg || slices == nullptr || sample == nullptr)
    {
        a = v.regionStart;
        b = v.regionEnd;
        return 0;
    }
    const double len = sample->length();
    for (int i = 0; i < slices->count; ++i)
    {
        const double sa = std::max (slices->s[(size_t) i].start * len, v.regionStart);
        const double sb = std::min (slices->s[(size_t) i].end * len, v.regionEnd);
        if (pos >= sa && pos < sb) { a = sa; b = sb; return i; }
    }
    return -1;   // a gap between slices: silence
}

void SliceEngine::jumpToSegment (Voice& v, double dir, const VoiceContext& c, bool loopWrap)
{
    double a = 0.0, b = 0.0;
    const int newSeg = segmentAt (v, v.timeline, a, b);
    const double newHead = dir > 0 ? a : b - 1e-6;

    if (newSeg >= 0 && v.seg >= 0 && ! v.bodyOn && std::abs (newHead - v.head) < 2.0)
    {
        // The audio simply carries on into the next slice (the tempo matches): no fades at all.
        v.seg = newSeg;
        v.segStart = a;
        v.segEnd = b;
        v.oldGain = 0.f;
        updateNextContiguous (v, dir);
        return;
    }

    // A stretched body that was playing fades out underneath the next slice's hit.
    if (v.bodyOn)
    {
        v.bodyOn = false;
        v.bodyFading = true;
        v.bodyFadeGain = v.bodyMix;
        v.bodyFadeStep = std::max (v.bodyFadeGain, 0.001f) / std::max (1.f, c.fadeLen);
        v.bodyMix = 0.f;
    }

    // A loop wrap is a straight splice (a loop cut on the beat runs on seamlessly); any other jump
    // lets the old segment fade out underneath the new one.
    v.oldHead = v.head;
    v.oldGain = (v.seg >= 0 && ! loopWrap) ? v.lastSegGain : 0.f;
    v.oldStep = v.oldGain / std::max (1.f, c.fadeLen);
    v.seg = newSeg;
    if (newSeg >= 0)
    {
        v.segStart = a;
        v.segEnd = b;
        v.head = newHead;
        v.segOffset = loopWrap && v.multiSeg && v.plays > 0 ? 1e9 : 0.0;
    }
    updateNextContiguous (v, dir);
}

void SliceEngine::updateNextContiguous (Voice& v, double dir)
{
    // Does audio carry straight on after this segment (the next slice starts exactly where this one ends)?
    v.nextContiguous = false;
    if (! v.multiSeg || v.seg < 0)
        return;
    const double edge = dir > 0 ? v.segEnd : v.segStart;
    const bool atRegionEdge = dir > 0 ? edge >= v.regionEnd - 1.0 : edge <= v.regionStart + 1.0;
    if (atRegionEdge)
    {
        v.nextContiguous = v.preview ? params.loop : v.loop;   // a loop splices straight back to the start
        return;
    }
    double a, b;
    const double probe = dir > 0 ? edge + 0.5 : edge - 1.5;
    v.nextContiguous = segmentAt (v, probe, a, b) >= 0 && (dir > 0 ? std::abs (a - edge) < 1.0 : std::abs (b - edge) < 1.0);
}

float SliceEngine::readBounded (const std::vector<float>& buf, double pos, double a, double b) const
{
    if (pos < a || pos >= b)
        return 0.f;
    const float g = (float) std::min (std::min (1.0, (pos - a) / 24.0), std::min (1.0, (b - pos) / 48.0));
    return readSample (buf, pos) * g;
}

void SliceEngine::startBody (Voice& v, const VoiceContext& c, double dir, double rate, double transpose)
{
    auto& slot = stretchers[(size_t) v.slot];
    auto& st = *slot.stretch;
    st.setTransposeFactor ((float) transpose);
    // Prime the stretcher from where the direct audio is now, so the two line up for the crossfade.
    const int seekLen = std::min ((int) slot.inL.size(), st.outputSeekLength ((float) rate));
    for (int k = 0; k < seekLen; ++k)
    {
        const double pos = v.head + k * dir;
        slot.inL[(size_t) k] = readBounded (sample->left, pos, v.segStart, v.segEnd);
        slot.inR[(size_t) k] = readBounded (sample->right, pos, v.segStart, v.segEnd);
    }
    float* in[2] = { slot.inL.data(), slot.inR.data() };
    st.outputSeek (in, seekLen);
    v.bodyHead = v.head + seekLen * dir;
    v.bodyRate = rate;
    v.fifoPos = tonalBlock;
    v.inputCarry = 0.0;
    v.bodyOn = true;
    v.bodyFading = false;
    v.bodyMix = 0.f;
    (void) c;
}

void SliceEngine::pullBody (Voice& v, const VoiceContext& c, double dir, double remainingOut, float& L, float& R)
{
    auto& slot = stretchers[(size_t) v.slot];
    auto& st = *slot.stretch;
    if (v.fifoPos >= tonalBlock)
    {
        if (v.bodyOn)
        {
            // Steer the rate so the rest of the slice finishes exactly when its slot does.
            const double heard = v.bodyHead - dir * (st.inputLatency() + v.bodyRate * st.outputLatency());
            const double left = dir > 0 ? v.segEnd - heard : heard - v.segStart;
            if (remainingOut > 256.0)
                v.bodyRate = std::clamp (left / remainingOut, 0.05, 4.0);
            st.setTransposeFactor ((float) std::clamp (c.pitchRatio * v.tuneRatio * c.modRate, 0.05, 16.0));
        }
        v.inputCarry += tonalBlock * v.bodyRate;
        const int ni = std::min ((int) v.inputCarry, (int) slot.inL.size());
        v.inputCarry -= ni;
        for (int k = 0; k < ni; ++k)
        {
            const double pos = v.bodyHead + k * dir;
            slot.inL[(size_t) k] = readBounded (sample->left, pos, v.segStart, v.segEnd);
            slot.inR[(size_t) k] = readBounded (sample->right, pos, v.segStart, v.segEnd);
        }
        v.bodyHead += ni * dir;
        float* in[2] = { slot.inL.data(), slot.inR.data() };
        float* out[2] = { slot.outL.data(), slot.outR.data() };
        st.process (in, ni, out, tonalBlock);
        v.fifoPos = 0;
    }
    L = slot.outL[(size_t) v.fifoPos];
    R = slot.outR[(size_t) v.fifoPos];
    ++v.fifoPos;
}

void SliceEngine::renderBeats (Voice& v, const VoiceContext& c, double dir, bool loop, float& L, float& R, bool& finished)
{
    const double tlRate = c.speed * c.modRate;                     // the groove follows the tempo...
    const double rd = c.pitchRatio * v.tuneRatio * c.modRate;      // ...each hit plays untouched (pitch = varispeed)
    const double xfade = std::max (32.0, (double) msToSamples (10.f));

    // Output samples until the groove moves on to the next slice
    double remainingOut = 0.0;
    if (v.seg >= 0 && tlRate > 1e-6)
        remainingOut = (dir > 0 ? v.segEnd - v.timeline : v.timeline - v.segStart) / tlRate;

    // After the attack, hand the rest of the slice to the spectral stretcher so it exactly fills its slot:
    // no gaps when slower, nothing chopped when faster, and the hit itself is never processed.
    if (v.slot >= 0 && v.seg >= 0 && ! v.bodyOn && ! v.bodyFading && tlRate > 1e-4
        && std::abs (rd - tlRate) > tlRate * 0.002)
    {
        const double attackSrc = msToSamples (25.f) * std::max (rd, 0.05);
        const double remainingSrc = dir > 0 ? v.segEnd - v.head : v.head - v.segStart;
        if (v.segOffset >= attackSrc && remainingOut > xfade * 2.0 && remainingSrc > 256.0)
            startBody (v, c, dir, std::clamp (remainingSrc / remainingOut, 0.05, 4.0), std::clamp (rd, 0.05, 16.0));
    }

    // direct (untouched) audio
    float segGain = 0.f, dL = 0.f, dR = 0.f;
    if (v.seg >= 0 && v.bodyMix < 1.f)
    {
        const double toEnd = dir > 0 ? v.segEnd - v.head : v.head - v.segStart;
        if (toEnd > 0.0)
        {
            const float fadeIn = (float) std::min (1.0, v.segOffset / (v.multiSeg ? 8.0 : (double) v.fadeInLen));   // keeps the attack
            // Fade out before the end only if the audio would run out before the groove moves on and
            // nothing takes over (no stretcher free, or the slice is too short to stretch).
            const bool runsOut = (rd > tlRate * 1.000001 || ! v.nextContiguous) && ! v.bodyOn;
            const float fadeOut = runsOut ? std::clamp ((float) toEnd / std::max (c.fadeLen, v.fadeOutLen), 0.f, 1.f) : 1.f;
            segGain = std::min (fadeIn, fadeOut);
            dL = readSample (sample->left, v.head) * segGain;
            dR = readSample (sample->right, v.head) * segGain;
        }
    }
    if (v.seg >= 0)
    {
        v.head += rd * dir;
        v.segOffset += rd;
    }

    // stretched body (and the tail of the previous slice's body fading out)
    float bL = 0.f, bR = 0.f;
    if (v.slot >= 0 && (v.bodyOn || v.bodyFading))
        pullBody (v, c, dir, remainingOut, bL, bR);

    // A pad's own fade-out lands at the end of its slot, whatever the tempo.
    if (! v.multiSeg && v.fadeOutLen > 0.f && v.seg >= 0)
    {
        const float g = std::clamp ((float) remainingOut / v.fadeOutLen, 0.f, 1.f);
        bL *= g; bR *= g;
        if (v.bodyOn) { dL *= g; dR *= g; }
    }

    if (v.bodyOn)
    {
        v.bodyMix = std::min (1.f, v.bodyMix + (float) (1.0 / xfade));
        L += dL * (1.f - v.bodyMix) + bL * v.bodyMix;
        R += dR * (1.f - v.bodyMix) + bR * v.bodyMix;
        v.lastSegGain = segGain * (1.f - v.bodyMix);
    }
    else
    {
        L += dL;
        R += dR;
        v.lastSegGain = segGain;
        if (v.bodyFading)
        {
            L += bL * v.bodyFadeGain;
            R += bR * v.bodyFadeGain;
            v.bodyFadeGain -= v.bodyFadeStep;
            if (v.bodyFadeGain <= 0.f) { v.bodyFading = false; v.bodyFadeGain = 0.f; }
        }
    }

    if (v.oldGain > 0.f)
    {
        L += readSample (sample->left, v.oldHead) * v.oldGain;
        R += readSample (sample->right, v.oldHead) * v.oldGain;
        v.oldHead += rd * dir;
        v.oldGain = std::max (0.f, v.oldGain - v.oldStep);
    }

    v.timeline += tlRate * dir;
    v.display = v.timeline;
    const bool past = dir > 0 ? v.timeline >= v.regionEnd : v.timeline < v.regionStart;
    if (past)
    {
        ++v.plays;
        if (loop && (v.repeats <= 0 || v.plays < v.repeats))
        {
            const double regionLen = v.regionEnd - v.regionStart;
            v.timeline += dir > 0 ? -regionLen : regionLen;
            jumpToSegment (v, dir, c, true);
        }
        else
            finished = true;
    }
    else if (v.multiSeg)
    {
        double a, b;
        if (segmentAt (v, v.timeline, a, b) != v.seg)
            jumpToSegment (v, dir, c, false);
    }
}

float SliceEngine::readTonalInput (const Voice& v, const std::vector<float>& buf, double pos, bool loop) const
{
    const double rs = v.regionStart, re = v.regionEnd, len = re - rs;
    if (loop && len > 1.0)
    {
        pos = rs + std::fmod (pos - rs, len);
        if (pos < rs) pos += len;
        return readSample (buf, pos);
    }
    if (pos < rs || pos >= re)
        return 0.f;
    const float g = (float) std::min (std::min (1.0, (pos - rs) / 24.0), std::min (1.0, (re - pos) / 48.0));
    return readSample (buf, pos) * g;
}

void SliceEngine::renderTonal (Voice& v, const VoiceContext& c, double dir, bool loop, float& L, float& R, bool& finished)
{
    auto& slot = stretchers[(size_t) v.slot];
    auto& st = *slot.stretch;
    if (v.fifoPos >= tonalBlock)
    {
        const double rate = std::clamp (c.speed * c.modRate, 0.0, 4.0);
        const double transpose = std::clamp (c.pitchRatio * v.tuneRatio * c.modRate, 0.05, 16.0);
        st.setTransposeFactor ((float) transpose);

        const bool loopNow = loop && (v.repeats <= 0 || v.plays < v.repeats);
        v.inputCarry += tonalBlock * rate;
        const int ni = std::min ((int) v.inputCarry, (int) slot.inL.size());
        v.inputCarry -= ni;
        for (int k = 0; k < ni; ++k)
        {
            const double pos = v.head + k * dir;
            slot.inL[(size_t) k] = readTonalInput (v, sample->left, pos, loopNow);
            slot.inR[(size_t) k] = readTonalInput (v, sample->right, pos, loopNow);
        }
        v.head += ni * dir;
        const double len = v.regionEnd - v.regionStart;
        if (loopNow && len > 1.0 && (v.head >= v.regionEnd + len || v.head < v.regionStart - len))
        {
            v.head = v.regionStart + std::fmod (v.head - v.regionStart, len);
            if (v.head < v.regionStart) v.head += len;
            ++v.plays;
        }

        float* in[2] = { slot.inL.data(), slot.inR.data() };
        float* out[2] = { slot.outL.data(), slot.outR.data() };
        st.process (in, ni, out, tonalBlock);
        v.fifoPos = 0;

        // what you hear lags the input by the stretcher's latency
        double heard = v.head - dir * (st.inputLatency() + rate * st.outputLatency());
        if (loopNow && len > 1.0)
        {
            heard = v.regionStart + std::fmod (heard - v.regionStart, len);
            if (heard < v.regionStart) heard += len;
        }
        v.display = heard;
        if (! loopNow && (dir > 0 ? heard >= v.regionEnd : heard < v.regionStart))
            finished = true;

        // pad fades, by where the heard audio is in the slice
        float target = 1.f;
        if (! loopNow && ! v.multiSeg)
        {
            const double fromStart = dir > 0 ? heard - v.regionStart : v.regionEnd - heard;
            const double toEnd = dir > 0 ? v.regionEnd - heard : heard - v.regionStart;
            const double fo = std::max ((double) c.fadeLen, (double) v.fadeOutLen);
            target = (float) std::clamp (std::min (toEnd / fo, v.fadeInLen > 8.f ? fromStart / v.fadeInLen : 1.0), 0.0, 1.0);
        }
        v.tonalEdgeStep = (target - v.tonalEdge) / (float) tonalBlock;
    }
    v.tonalEdge = std::clamp (v.tonalEdge + v.tonalEdgeStep, 0.f, 1.f);
    L = slot.outL[(size_t) v.fifoPos] * v.tonalEdge;
    R = slot.outR[(size_t) v.fifoPos] * v.tonalEdge;
    ++v.fifoPos;
}

void SliceEngine::stopPreview()
{
    const float relSamples = msToSamples (10.f);
    for (auto& v : voices)
        if (v.active && v.preview && ! v.releasing)
        {
            v.releasing = true;
            v.releaseStep = std::max (v.env, 0.001f) / relSamples;
        }
}

void SliceEngine::trigger (int key, int note, float velocity, int level, bool)
{
    if (key < 0 || key >= maxSlices)
        return;

    // Pad mute / solo
    bool anySolo = false;
    if (pads != nullptr)
        for (auto& p : *pads) anySolo = anySolo || p.solo;
    const auto& pd = pad (key);
    if (pd.mute || (anySolo && ! pd.solo))
        return;

    startVoice (key, note, velocity, level, false);
}

void SliceEngine::startVoice (int key, int note, float velocity, int level, bool forceOneShot)
{
    if (sample == nullptr || sample->length() < 16 || slices == nullptr)
        return;
    const int total = numSlices();
    if (total <= 0 || key < 0 || key >= maxSlices || (key >= total && level < 0))
        return;

    const auto& pd = pad (key);

    // Which slice: shuffle order, then slice motion.
    int base = sliceOrder[(size_t) key];
    if (base < 0 || base >= total) base = std::min (key, total - 1);
    int slice = base;
    if (pd.motion != motionOff && total > 1)
    {
        const int cycles = pd.cycles > 0 ? std::min (pd.cycles, total) : total;
        int step;
        if (pd.motion == motionNext)
            step = motionCounter[(size_t) key]++ % cycles;
        else
            step = (int) ((randomUni() * 0.5f + 0.5f) * cycles) % cycles;
        slice = (base + step) % total;
    }

    // Choke: mono cuts everything; poly cuts the same pad and its mute group.
    const float chokeSamples = std::max (msToSamples (2.f), msToSamples (params.xfadeMs));
    const int pressed = level >= 0 ? level : key;
    for (auto& v : voices)
        if (v.active && ! v.releasing
            && (! params.poly || v.uiKey == pressed || (pd.muteGroup > 0 && v.muteGroup == pd.muteGroup)))
        {
            v.releasing = true;
            v.releaseStep = std::max (v.env, 0.001f) / chokeSamples;
        }

    Voice* target = nullptr;
    for (auto& v : voices)
        if (! v.active) { target = &v; break; }
    if (target == nullptr)
    {
        target = &voices[0];
        for (auto& v : voices)
            if (v.age < target->age)
                target = &v;
    }

    if (target->slot >= 0) stretchers[(size_t) target->slot].used = false;

    const double len = sample->length();
    const auto& sl = slices->s[(size_t) slice];

    float vel = std::clamp (velocity, 0.f, 1.f);
    double semis = pd.tune;
    float cutoff = pd.cutoffHz;
    float decayMs = 0.f;
    if (level >= 0)
    {
        const float t = level / 15.f;
        switch (params.sixteenType)
        {
            case sixteenVelocity: vel = (level + 1) / 16.f; break;
            case sixteenTune:     semis += level - 8; break;                        // pad 9 = original pitch
            case sixteenFilter:   cutoff = std::min (cutoff, 150.f * std::pow (133.f, t)); break;   // 150 Hz .. 20 kHz
            case sixteenDecay:    decayMs = 40.f * std::pow (50.f, t); break;      // 40 ms .. 2 s
            default: break;
        }
    }
    else if (params.fullLevel)
        vel = 1.f;

    Voice& v = *target;
    v = Voice();
    v.active = true;
    v.key = key;
    v.uiKey = level >= 0 ? level : key;
    v.note = note;
    v.slice = slice;
    const float sens = std::clamp (params.velSens, 0.f, 1.f);
    v.velocity = (1.f - sens) + sens * vel;
    v.regionStart = std::clamp (sl.start * len + pd.startMs * 0.001 * sr, 0.0, len - 16.0);
    double end = params.full ? slices->regionEnd * len : sl.end * len + pd.endMs * 0.001 * sr;
    end += pd.tailMs * 0.001 * sr;
    v.regionEnd = std::clamp (end, v.regionStart + 16.0, len);
    v.fadeInLen = std::max (8.f, msToSamples (pd.fadeInMs));
    v.fadeOutLen = pd.fadeOutMs > 0.f ? msToSamples (pd.fadeOutMs) : 0.f;
    v.reverse = pd.reverse;
    v.oneShot = forceOneShot || pd.playMode == playOneShot;
    v.loop = params.loop || (pd.loop && pd.playMode == playNoteOn);
    v.repeats = pd.repeats;
    v.muteGroup = pd.muteGroup;
    v.tuneRatio = std::pow (2.0, semis / 12.0);
    const double angle = (std::clamp (pd.pan, -1.f, 1.f) + 1.0) * 0.25 * 3.141592653589793;
    const float level01 = std::pow (10.f, pd.levelDb / 20.f);
    v.gainL = (float) (std::cos (angle) * 1.41421356) * level01;
    v.gainR = (float) (std::sin (angle) * 1.41421356) * level01;
    if (decayMs > 0.f)
        v.decayStep = 1.f / std::max (1.f, msToSamples (decayMs));
    if (cutoff < 19900.f)
    {
        v.filtered = true;
        const float g = (float) std::tan (3.141592653589793 * std::clamp ((double) cutoff, 20.0, sr * 0.45) / sr);
        v.fk = 2.f - 1.96f * 0.15f;
        v.fa1 = 1.f / (1.f + g * (g + v.fk));
        v.fa2 = g * v.fa1;
        v.fa3 = g * v.fa2;
    }
    const bool backwards = v.reverse != perf[fxReverse];
    v.head = backwards ? v.regionEnd - 1.0 : v.regionStart;
    v.env = params.attackMs > 0.f ? 0.f : 1.f;
    v.age = ++voiceCounter;

    const double grain = std::max (256.0, (double) msToSamples (params.grainMs));
    v.grainAnchor[0] = v.head;
    v.grainAge[0] = 0.0;
    v.grainAnchor[1] = v.head;
    v.grainAge[1] = grain * 0.5;
    v.multiSeg = params.full;
    setupVoiceMode (v);
}

void SliceEngine::updateRollLength (int fx, double beatSamples)
{
    double beats = 0.25;
    switch (fx)
    {
        case fxRoll4:   beats = 1.0; break;
        case fxRoll8:   beats = 0.5; break;
        case fxRoll16:  beats = 0.25; break;
        case fxRoll32:  beats = 0.125; break;
        case fxRoll8T:  beats = 1.0 / 3.0; break;
        case fxRoll16T: beats = 1.0 / 6.0; break;
        case fxBurst:   beats = 0.5; break;
        default: break;
    }
    rollLen = std::clamp ((int) (beats * beatSamples), 64, (int) rollL.size());
}

void SliceEngine::process (float* left, float* right, int n, const Transport& t)
{
    const double hostBpm = (t.bpm > 1.0 && t.bpm < 999.0) ? t.bpm : 120.0;
    const double beatSamples = 60.0 / hostBpm * sr;

    double speed = 1.0;
    const double lb = params.loopTempo;
    if (params.sync && lb > 0.0)
        speed = std::clamp (hostBpm / lb, 0.25, 4.0);

    const double grainLen = std::max (256.0, (double) msToSamples (params.grainMs));
    const float edgeSamples = std::max (16.f, msToSamples (params.xfadeMs));
    const float attackSamples = std::max (1.f, msToSamples (params.attackMs));
    const float glideCoef = params.glideMs > 0.f ? std::exp (-1.f / msToSamples (params.glideMs)) : 0.f;
    if (! semisInit) { currentSemis = params.pitchSemis; semisInit = true; }

    // Roll selection: the most "extreme" held roll wins.
    int rollFx = -1;
    for (int fx : { fxBurst, fxRoll16T, fxRoll8T, fxRoll32, fxRoll16, fxRoll8, fxRoll4 })
        if (perf[(size_t) fx]) { rollFx = fx; break; }
    if (rollFx != rollActiveFx)
    {
        if (rollFx >= 0)
        {
            updateRollLength (rollFx, beatSamples);
            rollWrite = rollRead = 0;
            rollFilled = false;
            burstStage = burstRepeats = 0;
        }
        rollActiveFx = rollFx;
    }

    // Filter setup
    const bool sweepHeld = perf[fxSweep];
    const bool filterIdle = params.filterMode == 0 && params.cutoffHz >= 19990.f
                            && (params.lfoRate == 0 || params.lfoDepth <= 0.f)
                            && ! sweepHeld && sweepProgress <= 0.0;
    static constexpr double lfoBeats[6] = { 0.0, 4.0, 2.0, 1.0, 0.5, 0.25 };
    const double lfoCycleBeats = lfoBeats[std::clamp (params.lfoRate, 0, 5)];

    // Crusher
    const float bits = perf[fxCrush] ? std::min (params.bits, 6.f) : params.bits;
    const int ds = perf[fxCrush] ? std::max (params.downsample, 8) : std::max (1, params.downsample);
    const float quant = bits < 23.5f ? std::pow (2.f, bits - 1.f) : 0.f;

    // Delay
    static constexpr double delayBeats[6] = { 0.25, 0.5, 0.75, 1.0, 1.5, 2.0 };
    const float targetDelay = (float) std::clamp (delayBeats[std::clamp (params.delayTime, 0, 5)] * beatSamples,
                                                  32.0, (double) delayL.size() - 4.0);
    if (smoothDelay <= 0.f) smoothDelay = targetDelay;
    const float throwDecay = std::exp (-1.f / (float) (1.5 * sr));

    const float targetGain = std::pow (10.f, params.outGainDb / 20.f);
    const int dlen = (int) delayL.size();

    Voice* lead = nullptr;

    // Note Repeat grid (in beats), with MPC swing on every other step of straight rates.
    static constexpr double repeatBeats[7] = { 1.0, 0.5, 0.25, 0.125, 1.0 / 3.0, 1.0 / 6.0, 1.0 / 12.0 };
    const int rateIndex = std::clamp (params.repeatRate, 0, 6);
    const double stepBeats = repeatBeats[rateIndex];
    const double swingOffset = rateIndex <= 3 ? (std::clamp ((double) params.swing, 0.5, 0.75) - 0.5) * 2.0 : 0.0;
    auto tickPos = [&] (long long k) { return (double) k + ((k & 1) ? swingOffset : 0.0); };
    bool anyHeld = false;
    for (auto& h : held) anyHeld = anyHeld || (h.on && ! h.pendingNoteOff);

    for (int i = 0; i < n; ++i)
    {
        const double ppqNow = t.ppq + i / beatSamples;
        const bool synced = t.playing && t.hasPpq;
        sampleClock += 1.0;

        // --- note repeat ---------------------------------------------------------
        if (params.noteRepeat)
        {
            repeatPos += 1.0 / beatSamples;
            const double beatsNow = synced ? ppqNow : repeatPos;
            const double p = beatsNow / stepBeats;
            if (synced != repeatSynced || p < tickPos (nextTick - 1) - 0.5 || p > tickPos (nextTick) + 2.0)
            {
                nextTick = (long long) std::floor (p) + 1;   // transport moved: line up with the grid
                repeatSynced = synced;
            }
            if (p >= tickPos (nextTick))
            {
                ++nextTick;
                if (anyHeld)
                    for (int note = 0; note < 128; ++note)
                    {
                        auto& h = held[(size_t) note];
                        // skip a tick that lands right after the pad was hit
                        if (h.on && ! h.pendingNoteOff && sampleClock - h.lastTrigger > stepBeats * beatSamples * 0.4)
                        {
                            h.lastTrigger = sampleClock;
                            trigger (h.key, note, h.velocity, h.level, true);
                        }
                    }
            }
        }

        // --- playback modifiers --------------------------------------------------
        currentSemis = params.pitchSemis + (currentSemis - params.pitchSemis) * glideCoef;

        if (perf[fxTapeStop])
        {
            tapeStopProgress = std::min (1.0, tapeStopProgress + 1.0 / beatSamples);
            const double r = 1.0 - tapeStopProgress;
            tapeRate = r * r;
        }
        else
            tapeRate += (1.0 - tapeRate) * (1.0 - std::exp (-1.0 / (0.06 * sr)));

        if (perf[fxDive]) diveProgress = std::min (1.0, diveProgress + 1.0 / beatSamples);
        else              diveProgress = std::max (0.0, diveProgress - 1.0 / (0.05 * sr));

        if (sweepHeld) sweepProgress = std::min (1.0, sweepProgress + 1.0 / (2.0 * beatSamples));
        else           sweepProgress = std::max (0.0, sweepProgress - 1.0 / (0.1 * sr));

        double modRate = tapeRate;
        if (perf[fxHalf] != perf[fxDouble])
            modRate *= perf[fxHalf] ? 0.5 : 2.0;
        lastSpeed = speed;
        lastModRate = modRate;

        const double semis = currentSemis - 12.0 * diveProgress * diveProgress;
        const double pitchRatio = std::pow (2.0, semis / 12.0);
        const double headRate = speed * modRate;
        const double basePitch = pitchRatio * modRate;

        // --- voices --------------------------------------------------------------
        float mixL = 0.f, mixR = 0.f;
        bool anyActive = false;

        if (sample != nullptr)
        {
            VoiceContext ctx;
            ctx.speed = speed;
            ctx.modRate = modRate;
            ctx.pitchRatio = pitchRatio;
            ctx.fadeLen = edgeSamples;

            for (auto& v : voices)
            {
                if (! v.active)
                    continue;
                anyActive = true;
                const double dir = (v.reverse != perf[fxReverse]) ? -1.0 : 1.0;
                const bool loopNow = v.preview ? params.loop : v.loop;
                float sL = 0.f, sR = 0.f, edge = 1.f;
                bool finished = false;

                if (v.mode == modeBeats)
                    renderBeats (v, ctx, dir, loopNow, sL, sR, finished);
                else if (v.mode == modeTonal)
                    renderTonal (v, ctx, dir, loopNow, sL, sR, finished);
                else
                {
                    // Texture mode: the original grain reader (used as an effect)
                    const double grainPitch = basePitch * v.tuneRatio;
                    if (! v.granular)
                    {
                        v.granular = true;
                        v.grainAge[0] = 0.0;
                        v.grainAnchor[0] = v.head;
                        v.grainAge[1] = grainLen * 0.5;
                        v.grainAnchor[1] = v.head + grainLen * 0.5 * (grainPitch - headRate) * dir;
                    }
                    for (int g = 0; g < 2; ++g)
                    {
                        const double w = 0.5 - 0.5 * std::cos (twoPi * v.grainAge[g] / grainLen);
                        sL += (float) w * readSample (sample->left, v.grainAnchor[g]);
                        sR += (float) w * readSample (sample->right, v.grainAnchor[g]);
                        v.grainAnchor[g] += grainPitch * dir;
                        v.grainAge[g] += 1.0;
                        if (v.grainAge[g] >= grainLen)
                        {
                            v.grainAge[g] -= grainLen;
                            v.grainAnchor[g] = v.head + randomUni() * params.textureAmount * grainLen;
                        }
                    }
                    const double dStart = v.head - v.regionStart, dEnd = v.regionEnd - v.head;
                    edge = std::clamp (std::min ((float) dStart / std::max (edgeSamples, v.fadeInLen),
                                                 (float) dEnd / std::max (edgeSamples, v.fadeOutLen)), 0.f, 1.f);
                    if (v.preview && loopNow && (v.plays > 0 || dEnd < dStart))
                        edge = 1.f;
                    v.head += headRate * dir;
                    v.display = v.head;
                    const double regionLen = v.regionEnd - v.regionStart;
                    if ((dir > 0 && v.head >= v.regionEnd) || (dir < 0 && v.head < v.regionStart))
                    {
                        ++v.plays;
                        if (loopNow && (v.repeats <= 0 || v.plays < v.repeats))
                        {
                            const double shift = dir > 0 ? -regionLen : regionLen;
                            v.head += shift; v.grainAnchor[0] += shift; v.grainAnchor[1] += shift;
                        }
                        else
                            finished = true;
                    }
                }

                if (finished && ! v.releasing)
                {
                    v.releasing = true;
                    v.releaseStep = std::max (v.env, 0.001f) / std::max (1.f, msToSamples (3.f));
                }

                if (v.releasing)
                {
                    v.env -= v.releaseStep;
                    if (v.env <= 0.f) { freeVoice (v); continue; }
                }
                else if (v.env < 1.f)
                    v.env = std::min (1.f, v.env + 1.f / attackSamples);

                if (v.decayStep > 0.f)
                {
                    v.decayGain -= v.decayStep;
                    if (v.decayGain <= 0.f) { freeVoice (v); continue; }
                }

                if (v.filtered)
                {
                    float* io[2] = { &sL, &sR };
                    for (int c = 0; c < 2; ++c)
                    {
                        const float x = *io[c];
                        const float v3 = x - v.f2[c];
                        const float v1 = v.fa1 * v.f1[c] + v.fa2 * v3;
                        const float v2 = v.f2[c] + v.fa2 * v.f1[c] + v.fa3 * v3;
                        v.f1[c] = 2.f * v1 - v.f1[c];
                        v.f2[c] = 2.f * v2 - v.f2[c];
                        *io[c] = v2;
                    }
                }

                const float g = v.env * edge * v.velocity * v.decayGain;
                mixL += sL * g * v.gainL;
                mixR += sR * g * v.gainR;

                if (v.active && ! v.releasing && (lead == nullptr || v.age > lead->age))
                    lead = &v;
            }
        }

        // --- vinyl ---------------------------------------------------------------
        activity += ((anyActive ? 1.f : 0.f) - activity) * (anyActive ? 0.01f : 0.00015f);
        if (params.vinyl > 0.f && activity > 0.0005f)
        {
            hissLp += 0.25f * (randomUni() - hissLp);
            if (crackleEnv < 0.01f && (randomUni() * 0.5f + 0.5f) < params.vinyl * 0.0012f)
            {
                crackleEnv = 0.25f + 0.35f * (randomUni() * 0.5f + 0.5f);
                crackleSign = randomUni() > 0.f ? 1.f : -1.f;
            }
            const float crackle = crackleEnv * crackleSign;
            crackleEnv *= 0.55f;
            crackleSign = -crackleSign;
            const float noise = (hissLp * 0.025f + crackle * 0.35f) * params.vinyl * activity;
            mixL += noise;
            mixR += noise * 0.9f;
        }

        // --- beat repeat ---------------------------------------------------------
        float repL = 0.f, repR = 0.f;
        if (rollActiveFx >= 0 && ! rollFilled)
        {
            rollL[(size_t) rollWrite] = mixL;
            rollR[(size_t) rollWrite] = mixR;
            if (++rollWrite >= rollLen) { rollFilled = true; rollRead = 0; }
        }
        else if (rollFilled)
        {
            const float f = (float) std::min (64, rollLen / 4);
            const float w = std::clamp (std::min ((float) rollRead, (float) (rollLen - 1 - rollRead)) / f, 0.f, 1.f);
            repL = rollL[(size_t) rollRead] * w;
            repR = rollR[(size_t) rollRead] * w;
            if (++rollRead >= rollLen)
            {
                rollRead = 0;
                if (rollActiveFx == fxBurst && ++burstRepeats >= 2 && burstStage < 3)
                {
                    ++burstStage;
                    burstRepeats = 0;
                    rollLen = std::max (64, rollLen / 2);
                }
            }
        }
        const float rollTarget = (rollActiveFx >= 0 && rollFilled) ? 1.f : 0.f;
        rollMix += (rollTarget - rollMix) * 0.02f;
        if (rollActiveFx < 0 && rollMix < 0.0005f)
            rollFilled = false;
        float xL = mixL + (repL - mixL) * rollMix;
        float xR = mixR + (repR - mixR) * rollMix;

        // --- gate ----------------------------------------------------------------
        float gateTarget = 1.f;
        if (perf[fxGate])
        {
            double ph;
            if (synced) ph = frac (ppqNow * 4.0);
            else { gatePhase = frac (gatePhase + 4.0 / beatSamples); ph = gatePhase; }
            gateTarget = ph < 0.5 ? 1.f : 0.f;
        }
        gateGain += (gateTarget - gateGain) * 0.01f;
        xL *= gateGain;
        xR *= gateGain;

        // --- filter --------------------------------------------------------------
        if (! filterIdle)
        {
            double octs = 0.0;
            if (lfoCycleBeats > 0.0 && params.lfoDepth > 0.f)
            {
                double ph;
                if (synced) ph = frac (ppqNow / lfoCycleBeats);
                else { lfoPhase = frac (lfoPhase + 1.0 / (lfoCycleBeats * beatSamples)); ph = lfoPhase; }
                octs += std::sin (twoPi * ph) * params.lfoDepth * 3.0;
            }
            octs += (params.filterMode == 1 ? 5.0 : -5.0) * sweepProgress;
            const float target = (float) std::clamp (params.cutoffHz * std::pow (2.0, octs), 20.0, sr * 0.45);
            smoothCutoff += (target - smoothCutoff) * 0.004f;

            const float res = std::clamp (params.resonance + 0.35f * (float) sweepProgress, 0.f, 1.f);
            const float k = 2.f - 1.96f * res;
            const float g = (float) std::tan (3.141592653589793 * smoothCutoff / sr);
            const float a1 = 1.f / (1.f + g * (g + k)), a2 = g * a1, a3 = g * a2;

            float* io[2] = { &xL, &xR };
            for (int c = 0; c < 2; ++c)
            {
                const float x = *io[c];
                const float v3 = x - ic2[c];
                const float v1 = a1 * ic1[c] + a2 * v3;
                const float v2 = ic2[c] + a2 * ic1[c] + a3 * v3;
                ic1[c] = 2.f * v1 - ic1[c];
                ic2[c] = 2.f * v2 - ic2[c];
                *io[c] = params.filterMode == 0 ? v2 : params.filterMode == 1 ? x - k * v1 - v2 : v1;
            }
        }
        else
        {
            ic1[0] = ic1[1] = ic2[0] = ic2[1] = 0.f;
            smoothCutoff = params.cutoffHz;
        }

        // --- crusher -------------------------------------------------------------
        if (ds > 1)
        {
            if (holdCounter <= 0) { heldL = xL; heldR = xR; holdCounter = ds; }
            --holdCounter;
            xL = heldL;
            xR = heldR;
        }
        if (quant > 0.f)
        {
            xL = std::round (xL * quant) / quant;
            xR = std::round (xR * quant) / quant;
        }

        // --- delay ---------------------------------------------------------------
        smoothDelay += (targetDelay - smoothDelay) * 0.0005f;
        if (perf[fxThrow]) throwWet = 1.f;
        else               throwWet *= throwDecay;

        const bool send = params.delayMix > 0.0005f || perf[fxThrow];
        float fb = params.delayFeedback;
        if (throwWet > 0.01f) fb = std::max (fb, 0.6f);
        fb = std::min (fb, 0.95f);

        double rp = delayWrite - smoothDelay;
        if (rp < 0) rp += dlen;
        const int r0 = (int) rp;
        const int r1 = (r0 + 1) % dlen;
        const float rf = (float) (rp - r0);
        const float dL = delayL[(size_t) r0] + (delayL[(size_t) r1] - delayL[(size_t) r0]) * rf;
        const float dR = delayR[(size_t) r0] + (delayR[(size_t) r1] - delayR[(size_t) r0]) * rf;
        fbLowL += 0.4f * (dL - fbLowL);
        fbLowR += 0.4f * (dR - fbLowR);
        delayL[(size_t) delayWrite] = std::tanh ((send ? xL : 0.f) + fbLowL * fb);
        delayR[(size_t) delayWrite] = std::tanh ((send ? xR : 0.f) + fbLowR * fb);
        if (++delayWrite >= dlen) delayWrite = 0;

        const float wet = std::max (params.delayMix, throwWet);
        xL += dL * wet;
        xR += dR * wet;

        // --- out -----------------------------------------------------------------
        outGain += (targetGain - outGain) * 0.002f;
        xL *= outGain;
        xR *= outGain;
        if (! std::isfinite (xL) || ! std::isfinite (xR))
        {
            xL = xR = 0.f;
            reset();
        }
        left[i] = xL;
        right[i] = xR;
    }

    // UI state
    bool previewing = false;
    for (auto& v : voices)
        previewing = previewing || (v.active && v.preview && ! v.releasing);
    uiPreviewing = previewing;
    if (lead != nullptr && sample != nullptr && sample->length() > 0)
    {
        uiActiveSlice = lead->slice;
        uiActiveKey = lead->uiKey;
        uiPlayhead = (float) std::clamp (lead->display / sample->length(), 0.0, 1.0);
    }
    else
    {
        uiActiveSlice = -1;
        uiActiveKey = -1;
        uiPlayhead = -1.f;
    }
    uint32_t mask = 0;
    for (int fx = 0; fx < numPerfFx; ++fx)
        if (perf[(size_t) fx]) mask |= 1u << fx;
    uiPerfMask = mask;
}

} // namespace novi
