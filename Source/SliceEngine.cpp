#include "SliceEngine.h"

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
    reset();
}

void SliceEngine::reset()
{
    for (auto& v : voices)
        v = Voice();
    perf.fill (false);
    heldNotes.fill (false);
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
            v.active = false;
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
        double len = bar / slicesPerBar (p);
        // Never more than maxSlices across the loop.
        if (lengthSamples / len > maxSlices + 0.01)
            len = lengthSamples / maxSlices;
        return std::max (1.0, len);
    }
    return lengthSamples / sliceCount (p, lengthSamples, sampleRate);
}

int SliceEngine::sliceCount (const EngineParams& p, double lengthSamples, double sampleRate)
{
    if (p.sampleBpm >= minSampleBpm && lengthSamples >= 2)
    {
        const double len = sliceLengthOf (p, lengthSamples, sampleRate);
        // Ignore a sliver of tail shorter than 10% of a slice.
        return std::clamp ((int) std::ceil (lengthSamples / len - 0.1), 1, maxSlices);
    }
    return std::clamp (std::max (1, p.bars) * slicesPerBar (p), 1, maxSlices);
}

int SliceEngine::numSlices() const
{
    return sliceCount (params, sample != nullptr ? sample->length() : 0, sr);
}

double SliceEngine::loopBpm() const
{
    return tempoOf (params, sample != nullptr ? sample->length() : 0, sr);
}

double SliceEngine::sliceLength() const
{
    return sliceLengthOf (params, sample != nullptr ? sample->length() : 0, sr);
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

    const int key = sliceKeyForNote (note);
    if (key >= 0 && key < numSlices())
    {
        heldNotes[(size_t) note] = true;
        startVoice (key, note, velocity);
    }
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

    heldNotes[(size_t) note] = false;
    const float relSamples = std::max (msToSamples (5.f), msToSamples (params.releaseMs));
    for (auto& v : voices)
        if (v.active && ! v.releasing && v.note == note)
        {
            v.releasing = true;
            v.releaseStep = std::max (v.env, 0.001f) / relSamples;
        }
}

void SliceEngine::allNotesOff()
{
    for (int n = 0; n < 128; ++n)
        if (heldNotes[(size_t) n])
            noteOff (n);
    perf.fill (false);
}

void SliceEngine::startVoice (int key, int note, float velocity)
{
    if (sample == nullptr || sample->length() < 16)
        return;

    // Choke whatever is playing (mono slicer), with a short crossfade.
    const float chokeSamples = std::max (msToSamples (2.f), msToSamples (params.xfadeMs));
    for (auto& v : voices)
        if (v.active && ! v.releasing)
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

    const int total = numSlices();
    int slice = sliceOrder[(size_t) key];
    if (slice < 0 || slice >= total)
        slice = key;

    const double len = sample->length();
    const double sliceLen = sliceLength();

    Voice& v = *target;
    v = Voice();
    v.active = true;
    v.key = key;
    v.note = note;
    v.slice = slice;
    v.velocity = std::clamp (velocity, 0.f, 1.f);
    v.regionStart = std::min (slice * sliceLen, len - 16.0);
    v.regionEnd = params.full ? len : std::min (len, (slice + 1) * sliceLen);
    v.head = perf[fxReverse] ? v.regionEnd - 1.0 : v.regionStart;
    v.env = params.attackMs > 0.f ? 0.f : 1.f;
    v.age = ++voiceCounter;

    const double grain = std::max (256.0, (double) msToSamples (params.grainMs));
    v.grainAnchor[0] = v.head;
    v.grainAge[0] = 0.0;
    v.grainAnchor[1] = v.head;
    v.grainAge[1] = grain * 0.5;
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
    const double lb = loopBpm();
    if (params.sync && lb > 0.0)
        speed = std::clamp (hostBpm / lb, 0.25, 4.0);

    const bool granular = params.keyLock || params.textureAmount > 0.001f;
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

    for (int i = 0; i < n; ++i)
    {
        const double ppqNow = t.ppq + i / beatSamples;
        const bool synced = t.playing && t.hasPpq;

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

        const double semis = currentSemis - 12.0 * diveProgress * diveProgress;
        const double pitchRatio = std::pow (2.0, semis / 12.0);
        const double dir = perf[fxReverse] ? -1.0 : 1.0;
        const double headRate = params.keyLock ? speed * modRate : speed * pitchRatio * modRate;
        const double grainPitch = params.keyLock ? pitchRatio * modRate : headRate;

        // --- voices --------------------------------------------------------------
        float mixL = 0.f, mixR = 0.f;
        bool anyActive = false;

        if (sample != nullptr)
        {
            for (auto& v : voices)
            {
                if (! v.active)
                    continue;
                anyActive = true;

                float sL, sR;
                if (granular)
                {
                    sL = sR = 0.f;
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
                }
                else
                {
                    sL = readSample (sample->left, v.head);
                    sR = readSample (sample->right, v.head);
                }

                const double dStart = v.head - v.regionStart, dEnd = v.regionEnd - v.head;
                const float edge = std::clamp ((float) std::min (dStart, dEnd) / edgeSamples, 0.f, 1.f);

                if (v.releasing)
                {
                    v.env -= v.releaseStep;
                    if (v.env <= 0.f) { v.active = false; continue; }
                }
                else if (v.env < 1.f)
                    v.env = std::min (1.f, v.env + 1.f / attackSamples);

                const float g = v.env * edge * v.velocity;
                mixL += sL * g;
                mixR += sR * g;

                v.head += headRate * dir;
                const double regionLen = v.regionEnd - v.regionStart;
                if (dir > 0 && v.head >= v.regionEnd)
                {
                    if (params.loop) { v.head -= regionLen; v.grainAnchor[0] -= regionLen; v.grainAnchor[1] -= regionLen; }
                    else v.active = false;
                }
                else if (dir < 0 && v.head < v.regionStart)
                {
                    if (params.loop) { v.head += regionLen; v.grainAnchor[0] += regionLen; v.grainAnchor[1] += regionLen; }
                    else v.active = false;
                }

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
    if (lead != nullptr && sample != nullptr && sample->length() > 0)
    {
        uiActiveSlice = lead->slice;
        uiActiveKey = lead->key;
        uiPlayhead = (float) std::clamp (lead->head / sample->length(), 0.0, 1.0);
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
