#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <numeric>

using APVTS = juce::AudioProcessorValueTreeState;

const juce::StringArray NoviSliceProcessor::barsChoices   { "1", "2", "3", "4", "5", "6", "7", "8", "16" };
const juce::StringArray NoviSliceProcessor::slicesChoices { "1", "2", "4", "8" };
const juce::StringArray NoviSliceProcessor::lfoChoices    { "OFF", "1/1", "1/2", "1/4", "1/8", "1/16" };
const juce::StringArray NoviSliceProcessor::modeChoices   { "LP", "HP", "BP" };
const juce::StringArray NoviSliceProcessor::delayChoices  { "1/16", "1/8", "1/8D", "1/4", "1/4D", "1/2" };
const juce::StringArray NoviSliceProcessor::chopChoices   { "BPM GRID", "REGIONS", "TRANSIENT", "MANUAL" };
const juce::StringArray NoviSliceProcessor::polyChoices   { "MONO", "POLY" };
const juce::StringArray NoviSliceProcessor::repeatChoices { "1/4", "1/8", "1/16", "1/32", "1/8T", "1/16T", "1/32T" };
const juce::StringArray NoviSliceProcessor::sixteenChoices { "VELOCITY", "TUNE", "FILTER", "DECAY" };

namespace
{
    juce::AudioParameterFloatAttributes withText (std::function<juce::String (float)> f)
    {
        return juce::AudioParameterFloatAttributes().withStringFromValueFunction ([f] (float v, int) { return f (v); });
    }

    juce::String pct (float v)  { return juce::String (juce::roundToInt (v)) + " %"; }
    juce::String ms (float v)   { return v < 0.5f ? juce::String ("OFF") : v >= 1000.f ? juce::String (v / 1000.f, 2) + " s" : juce::String (juce::roundToInt (v)) + " ms"; }
    juce::String msPlain (float v) { return juce::String (v, v < 10.f ? 1 : 0) + " ms"; }

    std::unique_ptr<juce::AudioParameterFloat> floatParam (const juce::String& id, const juce::String& name,
                                                           juce::NormalisableRange<float> range, float def,
                                                           std::function<juce::String (float)> text)
    {
        return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, range, def, withText (text));
    }

    juce::NormalisableRange<float> skewed (float lo, float hi, float centre, float step = 0.f)
    {
        juce::NormalisableRange<float> r (lo, hi, step);
        r.setSkewForCentre (centre);
        return r;
    }
}

APVTS::ParameterLayout NoviSliceProcessor::createLayout()
{
    APVTS::ParameterLayout l;
    auto choice = [&] (const juce::String& id, const juce::String& name, const juce::StringArray& c, int def)
    { l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, c, def)); };
    auto toggle = [&] (const juce::String& id, const juce::String& name, bool def)
    { l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def)); };

    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "sampleBpm", 1 }, "Loop BPM",
        juce::NormalisableRange<float> (0.f, novi::maxSampleBpm, 0.01f), 0.f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int) { return v < novi::minSampleBpm ? juce::String ("AUTO") : juce::String (v, 1); })
            .withValueFromStringFunction ([] (const juce::String& t)
            {
                const float v = t.trim().getFloatValue();
                return v >= novi::minSampleBpm ? juce::jmin (v, novi::maxSampleBpm) : 0.f;
            })));
    choice ("bars", "Bars", barsChoices, 3);
    choice ("slices", "Slices per bar", slicesChoices, 2);
    toggle ("triplet", "Triplet", false);
    toggle ("full", "Full", false);
    toggle ("sync", "Sync", true);
    toggle ("keylock", "Key lock", false);
    toggle ("loop", "Loop", false);

    l.add (floatParam ("texAmount", "Texture", { 0.f, 100.f, 0.1f }, 0.f, pct));
    l.add (floatParam ("texSize", "Grain size", skewed (20.f, 250.f, 90.f, 0.1f), 60.f, msPlain));
    l.add (floatParam ("vinyl", "Vinyl", { 0.f, 100.f, 0.1f }, 0.f, pct));

    l.add (floatParam ("pitch", "Pitch", { -24.f, 24.f, 1.f }, 0.f,
                       [] (float v) { const int s = juce::roundToInt (v); return (s > 0 ? "+" : "") + juce::String (s) + " st"; }));
    l.add (floatParam ("glide", "Glide", skewed (0.f, 500.f, 80.f, 0.1f), 0.f, ms));
    l.add (floatParam ("xfade", "Crossfade", skewed (0.f, 50.f, 8.f, 0.1f), 4.f, msPlain));

    l.add (floatParam ("cutoff", "Cutoff", skewed (20.f, 20000.f, 1000.f, 0.1f), 20000.f,
                       [] (float v) { return v >= 1000.f ? juce::String (v / 1000.f, 1) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz"; }));
    l.add (floatParam ("res", "Resonance", { 0.f, 100.f, 0.1f }, 10.f, pct));
    choice ("lfoRate", "Filter LFO rate", lfoChoices, 0);
    l.add (floatParam ("lfoDepth", "Filter LFO depth", { 0.f, 100.f, 0.1f }, 0.f, pct));
    choice ("mode", "Filter mode", modeChoices, 0);

    l.add (floatParam ("bits", "Bits", { 4.f, 24.f, 1.f }, 24.f,
                       [] (float v) { return v >= 23.5f ? juce::String ("OFF") : juce::String (juce::roundToInt (v)) + " bit"; }));
    l.add (floatParam ("downsample", "Downsample", { 1.f, 32.f, 1.f }, 1.f,
                       [] (float v) { const int d = juce::roundToInt (v); return d <= 1 ? juce::String ("OFF") : "1/" + juce::String (d); }));

    l.add (floatParam ("dlyMix", "Delay mix", { 0.f, 100.f, 0.1f }, 0.f, pct));
    l.add (floatParam ("dlyFb", "Delay feedback", { 0.f, 95.f, 0.1f }, 35.f, pct));
    choice ("dlyTime", "Delay time", delayChoices, 3);

    l.add (floatParam ("attack", "Attack", skewed (0.f, 500.f, 60.f, 0.1f), 0.f, ms));
    l.add (floatParam ("release", "Release", skewed (0.f, 2000.f, 200.f, 0.1f), 0.f, ms));
    l.add (floatParam ("gain", "Output", { -24.f, 12.f, 0.1f }, 0.f,
                       [] (float v) { return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB"; }));

    // Chop (MPC Chop Mode)
    choice ("chopMode", "Chop mode", chopChoices, 0);
    l.add (floatParam ("regions", "Regions", { 1.f, 32.f, 1.f }, 16.f, [] (float v) { return juce::String (juce::roundToInt (v)); }));
    l.add (floatParam ("sensitivity", "Transient sensitivity", { 0.f, 100.f, 0.1f }, 50.f, pct));
    l.add (floatParam ("minTime", "Transient min time", skewed (10.f, 500.f, 80.f, 0.1f), 60.f, msPlain));
    l.add (floatParam ("trimStart", "Trim start", { 0.f, 1.f, 0.00001f }, 0.f, [] (float v) { return juce::String (v * 100.f, 1) + " %"; }));
    l.add (floatParam ("trimEnd", "Trim end", { 0.f, 1.f, 0.00001f }, 1.f, [] (float v) { return juce::String (v * 100.f, 1) + " %"; }));
    toggle ("zeroSnap", "Zero snap", false);
    toggle ("linkSlices", "Link slices", true);

    // Play modes (MPC pad / program settings)
    choice ("poly", "Polyphony", polyChoices, 0);
    toggle ("fullLevel", "Full level", false);
    l.add (floatParam ("velSens", "Velocity sensitivity", { 0.f, 100.f, 0.1f }, 100.f, pct));
    toggle ("noteRepeat", "Note repeat", false);
    choice ("repeatRate", "Note repeat rate", repeatChoices, 2);
    l.add (floatParam ("swing", "Swing", { 50.f, 75.f, 0.1f }, 50.f, [] (float v) { return juce::String (v, 1) + " %"; }));
    toggle ("sixteen", "16 levels", false);
    choice ("sixteenType", "16 levels type", sixteenChoices, 0);
    l.add (floatParam ("sixteenSource", "16 levels pad", { 1.f, 32.f, 1.f }, 1.f, [] (float v) { return "PAD " + juce::String (juce::roundToInt (v)); }));
    return l;
}

NoviSliceProcessor::NoviSliceProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "NoviSlice", createLayout())
{
    formats.registerBasicFormats();
    for (int i = 0; i < novi::maxSlices; ++i)
        order[(size_t) i].store (i);
    lastChopHash = chopHash();
    startTimerHz (15);
}

NoviSliceProcessor::~NoviSliceProcessor()
{
    stopTimer();
}

bool NoviSliceProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void NoviSliceProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const bool rateChanged = std::abs (sampleRate - currentRate) > 0.5;
    currentRate = sampleRate;
    currentBlock = samplesPerBlock;
    {
        const juce::SpinLock::ScopedLockType sl (playbackLock);
        engine.prepare (sampleRate, samplesPerBlock);
    }
    if (rateChanged || playback == nullptr)
        rebuildPlaybackData();
    keyboardState.reset();
}

void NoviSliceProcessor::readParams (novi::EngineParams& p) const
{
    auto f = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    auto i = [&] (const char* id) { return juce::roundToInt (f (id)); };
    static constexpr int pow2[4] = { 1, 2, 4, 8 };
    p.loopTempo = loopTempo.load();
    p.sampleBpm = f ("sampleBpm");
    p.bars = barsChoices[juce::jlimit (0, barsChoices.size() - 1, i ("bars"))].getIntValue();
    p.slicesPerBar = pow2[juce::jlimit (0, 3, i ("slices"))];
    p.triplet = f ("triplet") > 0.5f;
    p.full = f ("full") > 0.5f;
    p.sync = f ("sync") > 0.5f;
    p.keyLock = f ("keylock") > 0.5f;
    p.loop = f ("loop") > 0.5f;
    p.textureAmount = f ("texAmount") / 100.f;
    p.grainMs = f ("texSize");
    p.vinyl = f ("vinyl") / 100.f;
    p.pitchSemis = f ("pitch");
    p.glideMs = f ("glide");
    p.xfadeMs = f ("xfade");
    p.cutoffHz = f ("cutoff");
    p.resonance = f ("res") / 100.f;
    p.lfoRate = i ("lfoRate");
    p.lfoDepth = f ("lfoDepth") / 100.f;
    p.filterMode = i ("mode");
    p.bits = f ("bits");
    p.downsample = i ("downsample");
    p.delayMix = f ("dlyMix") / 100.f;
    p.delayFeedback = f ("dlyFb") / 100.f;
    p.delayTime = i ("dlyTime");
    p.attackMs = f ("attack");
    p.releaseMs = f ("release");
    p.outGainDb = f ("gain");
    p.poly = i ("poly") == 1;
    p.fullLevel = f ("fullLevel") > 0.5f;
    p.velSens = f ("velSens") / 100.f;
    p.noteRepeat = f ("noteRepeat") > 0.5f;
    p.repeatRate = i ("repeatRate");
    p.swing = f ("swing") / 100.f;
    p.sixteenLevels = f ("sixteen") > 0.5f;
    p.sixteenType = i ("sixteenType");
    p.sixteenSource = juce::jlimit (0, novi::maxSlices - 1, i ("sixteenSource") - 1);
}

void NoviSliceProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();

    keyboardState.processNextMidiBuffer (midi, 0, n, true);

    const juce::SpinLock::ScopedTryLockType sl (playbackLock);
    if (! sl.isLocked())
        return;

    novi::EngineParams p;
    readParams (p);
    engine.setParams (p);
    engine.setSample (playback.get());
    engine.setSlices (&engineMap);
    engine.setPads (&pads);

    std::array<int, novi::maxSlices> ord {};
    for (int k = 0; k < novi::maxSlices; ++k)
        ord[(size_t) k] = order[(size_t) k].load();
    engine.setOrder (ord);

    if (const int req = previewRequest.exchange (0); req == 1)
        engine.startPreview();
    else if (req == 2)
        engine.stopPreview();

    novi::Transport t;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm()) { t.bpm = *bpm; hostBpm.store (*bpm); }
            t.playing = pos->getIsPlaying();
            if (auto ppq = pos->getPpqPosition()) { t.ppq = *ppq; t.hasPpq = true; }
        }

    float* left = buffer.getWritePointer (0);
    float* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    std::vector<float> scratch;
    if (right == nullptr) { scratch.resize ((size_t) n); right = scratch.data(); }

    const double samplesPerBeat = 60.0 / juce::jmax (1.0, t.bpm) * getSampleRate();
    int pos = 0;
    auto renderTo = [&] (int end)
    {
        if (end > pos)
        {
            novi::Transport seg = t;
            seg.ppq = t.ppq + pos / samplesPerBeat;
            engine.process (left + pos, right + pos, end - pos, seg);
            pos = end;
        }
    };

    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        renderTo (juce::jlimit (0, n, meta.samplePosition));
        if (msg.isNoteOn())
            engine.noteOn (msg.getNoteNumber(), msg.getFloatVelocity());
        else if (msg.isNoteOff())
            engine.noteOff (msg.getNoteNumber());
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            engine.allNotesOff();
    }
    renderTo (n);

    if (buffer.getNumChannels() == 1)
        for (int i = 0; i < n; ++i)
            left[i] = 0.5f * (left[i] + right[i]);

    outputPeak.store (juce::jmax (outputPeak.load() * 0.8f, buffer.getMagnitude (0, n)));
}

// --- sample loading ------------------------------------------------------------

bool NoviSliceProcessor::loadFile (const juce::File& file)
{
    juce::MemoryBlock data;
    if (! file.existsAsFile() || file.getSize() > 200 * 1024 * 1024 || ! file.loadFileAsData (data))
        return false;
    return loadFromMemory (data, file.getFileNameWithoutExtension(), file.getFullPathName(), true);
}

static void setParam (juce::AudioProcessorValueTreeState& apvts, const char* id, float value)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        p->endChangeGesture();
    }
}

bool NoviSliceProcessor::loadFromMemory (const juce::MemoryBlock& data, const juce::String& name,
                                         const juce::String& path, bool fresh)
{
    auto stream = std::make_unique<juce::MemoryInputStream> (data, false);
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (std::move (stream)));
    if (reader == nullptr || reader->lengthInSamples < 64 || reader->sampleRate <= 0)
        return false;

    const auto maxLen = (juce::int64) (reader->sampleRate * 60.0);   // one minute is plenty for a loop
    const int len = (int) juce::jmin (reader->lengthInSamples, maxLen);
    juce::AudioBuffer<float> buf (2, len);
    reader->read (&buf, 0, len, 0, true, reader->numChannels > 1);
    if (reader->numChannels == 1)
        buf.copyFrom (1, 0, buf, 0, 0, len);

    {
        const juce::ScopedLock l (sampleInfoLock);
        original = std::move (buf);
        originalRate = reader->sampleRate;
        fileData = data;
        sampleName = name;
        samplePath = path;
    }

    if (fresh)
    {
        // Pick the bar count whose tempo matches a BPM in the file name, or lands nearest 100 BPM.
        const double seconds = len / reader->sampleRate;
        double wanted = 0.0;
        for (auto token : juce::StringArray::fromTokens (name.toLowerCase().replace ("bpm", " "), " _-.()[]", ""))
        {
            const double v = token.getDoubleValue();
            if (token.containsOnly ("0123456789.") && v >= 50.0 && v <= 200.0) { wanted = v; break; }
        }
        int best = barsChoices.indexOf ("4"); double bestErr = 1e9;
        for (int idx = 0; idx < barsChoices.size(); ++idx)
        {
            const int barCount = barsChoices[idx].getIntValue();
            if (barCount != 1 && barCount != 2 && barCount != 4 && barCount != 8)
                continue;   // loops are almost always 1, 2, 4 or 8 bars
            const double bpm = barCount * 4.0 * 60.0 / seconds;
            const double err = wanted > 0.0 ? std::abs (bpm - wanted) : std::abs (std::log2 (bpm / 100.0));
            if (err < bestErr) { bestErr = err; best = idx; }
        }
        setParam (apvts, "bars", (float) best);
        // A BPM in the file name becomes the Loop BPM; otherwise go back to auto.
        setLoopBpm (wanted > 0.0 ? wanted : 0.0);
        setParam (apvts, "trimStart", 0.f);
        setParam (apvts, "trimEnd", 1.f);
        manualSlices.clear();
        if (getChopMode() == novi::chopManual)
            setParam (apvts, "chopMode", (float) novi::chopGrid);
        resetOrder();
    }

    audioChanged (false);
    return true;
}

void NoviSliceProcessor::audioChanged (bool encodeFile)
{
    std::vector<float> mono;
    {
        const juce::ScopedLock l (sampleInfoLock);
        const int len = original.getNumSamples();

        if (encodeFile && len > 0)
        {
            // Processed audio replaces the stored file, so the project reopens with the edit.
            juce::MemoryBlock block;
            {
                auto out = std::make_unique<juce::MemoryOutputStream> (block, false);
                juce::WavAudioFormat wav;
                std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (out.get(), originalRate, 2, 24, {}, 0));
                if (writer != nullptr)
                {
                    out.release();
                    writer->writeFromAudioSampleBuffer (original, 0, len);
                }
            }
            fileData = block;
        }

        mono.resize ((size_t) len);
        for (int i = 0; i < len; ++i)
            mono[(size_t) i] = 0.5f * (original.getSample (0, i) + original.getSample (1, i));

        std::vector<float> newPeaks ((size_t) numPeakColumns * 2, 0.f);
        for (int c = 0; c < numPeakColumns && len > 0; ++c)
        {
            const int a = (int) ((juce::int64) c * len / numPeakColumns);
            const int b = juce::jmax (a + 1, (int) ((juce::int64) (c + 1) * len / numPeakColumns));
            float lo = 0.f, hi = 0.f;
            for (int i = a; i < b && i < len; ++i)
            {
                lo = juce::jmin (lo, mono[(size_t) i]);
                hi = juce::jmax (hi, mono[(size_t) i]);
            }
            newPeaks[(size_t) c * 2] = lo;
            newPeaks[(size_t) c * 2 + 1] = hi;
        }
        peaks = std::move (newPeaks);
        chopper.analyse (mono.data(), len, originalRate);
    }
    ++sampleVersion;
    rebuildPlaybackData();
    recomputeSlices();
}

void NoviSliceProcessor::rebuildPlaybackData()
{
    auto data = std::make_unique<novi::SampleData>();
    {
        const juce::ScopedLock l (sampleInfoLock);
        const int inLen = original.getNumSamples();
        if (inLen > 0)
        {
            const double ratio = originalRate / currentRate;
            const int outLen = juce::jmax (1, (int) (inLen / ratio));
            data->left.resize ((size_t) outLen);
            data->right.resize ((size_t) outLen);
            std::vector<float>* outs[2] = { &data->left, &data->right };
            for (int c = 0; c < 2; ++c)
            {
                if (std::abs (ratio - 1.0) < 1e-9)
                    std::copy (original.getReadPointer (c), original.getReadPointer (c) + juce::jmin (inLen, outLen), outs[c]->begin());
                else
                {
                    juce::LagrangeInterpolator interp;
                    interp.process (ratio, original.getReadPointer (c), outs[c]->data(), outLen, inLen, 0);
                }
            }
        }
    }

    std::unique_ptr<novi::SampleData> old;
    {
        const juce::SpinLock::ScopedLockType sl (playbackLock);
        engine.setSample (data->length() > 0 ? data.get() : nullptr);
        old = std::move (playback);
        playback = data->length() > 0 ? std::move (data) : nullptr;
    }
    // `old` is freed here, outside the audio thread.
}

juce::String NoviSliceProcessor::getSampleName() const  { const juce::ScopedLock l (sampleInfoLock); return sampleName; }
bool NoviSliceProcessor::hasSample() const              { const juce::ScopedLock l (sampleInfoLock); return original.getNumSamples() > 0; }
std::vector<float> NoviSliceProcessor::getPeaks() const { const juce::ScopedLock l (sampleInfoLock); return peaks; }

double NoviSliceProcessor::getSampleSeconds() const
{
    const juce::ScopedLock l (sampleInfoLock);
    return original.getNumSamples() / originalRate;
}

// --- slices ------------------------------------------------------------------------

novi::ChopSettings NoviSliceProcessor::currentChopSettings() const
{
    novi::ChopSettings c;
    readParams (c.grid);
    auto f = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    c.mode = juce::jlimit (0, novi::numChopModes - 1, juce::roundToInt (f ("chopMode")));
    c.regions = juce::roundToInt (f ("regions"));
    c.sensitivity = f ("sensitivity") / 100.f;
    c.minTimeMs = f ("minTime");
    c.trimStart = f ("trimStart");
    c.trimEnd = f ("trimEnd");
    c.zeroSnap = f ("zeroSnap") > 0.5f;
    return c;
}

juce::String NoviSliceProcessor::chopHash() const
{
    juce::String h;
    for (auto id : { "chopMode", "regions", "sensitivity", "minTime", "trimStart", "trimEnd", "zeroSnap",
                     "sampleBpm", "bars", "slices", "triplet" })
        h << apvts.getRawParameterValue (id)->load() << ";";
    return h;
}

void NoviSliceProcessor::timerCallback()
{
    if (chopHash() != lastChopHash)
        recomputeSlices();
}

void NoviSliceProcessor::recomputeSlices()
{
    const auto cs = currentChopSettings();
    novi::SliceMap map;
    double tempo = 0.0;
    {
        const juce::ScopedLock l (sampleInfoLock);
        if (original.getNumSamples() > 0)
        {
            map = chopper.compute (cs, manualSlices);
            tempo = chopper.tempo (cs);
        }
        uiMap = map;
    }
    {
        const juce::SpinLock::ScopedLockType sl (playbackLock);
        engineMap = map;
    }
    loopTempo.store (tempo);
    lastChopHash = chopHash();
    ++sliceVersion;
}

novi::SliceMap NoviSliceProcessor::getSliceMap() const { const juce::ScopedLock l (sampleInfoLock); return uiMap; }
int NoviSliceProcessor::getNumSlices() const           { const juce::ScopedLock l (sampleInfoLock); return uiMap.count; }
int NoviSliceProcessor::getChopMode() const            { return juce::roundToInt (apvts.getRawParameterValue ("chopMode")->load()); }

double NoviSliceProcessor::getLoopBpm() const
{
    const double t = loopTempo.load();
    if (t > 0.0) return t;
    const float set = apvts.getRawParameterValue ("sampleBpm")->load();
    return set >= novi::minSampleBpm ? (double) set : 0.0;
}

bool NoviSliceProcessor::isLoopBpmManual() const
{
    return apvts.getRawParameterValue ("sampleBpm")->load() >= novi::minSampleBpm;
}

void NoviSliceProcessor::setLoopBpm (double bpm)
{
    setParam (apvts, "sampleBpm", bpm >= novi::minSampleBpm
                                      ? (float) juce::jlimit ((double) novi::minSampleBpm, (double) novi::maxSampleBpm, bpm) : 0.f);
}

void NoviSliceProcessor::ensureManual()
{
    if (getChopMode() == novi::chopManual)
        return;
    const auto map = getSliceMap();
    manualSlices.clear();
    for (int i = 0; i < map.count; ++i)
        manualSlices.push_back (map.s[(size_t) i]);
    setParam (apvts, "chopMode", (float) novi::chopManual);
}

void NoviSliceProcessor::commitManual()
{
    std::sort (manualSlices.begin(), manualSlices.end(), [] (const novi::Slice& a, const novi::Slice& b) { return a.start < b.start; });
    recomputeSlices();
}

void NoviSliceProcessor::moveSliceEdge (int slice, bool startEdge, double fraction)
{
    ensureManual();
    if (slice < 0 || slice >= (int) manualSlices.size())
        return;
    const double minLen = 0.01 * originalRate / juce::jmax (1.0, getSampleSeconds() * originalRate);   // 10 ms
    const bool link = apvts.getRawParameterValue ("linkSlices")->load() > 0.5f;
    if (apvts.getRawParameterValue ("zeroSnap")->load() > 0.5f)
        fraction = chopper.snapToZero (fraction);

    auto& s = manualSlices[(size_t) slice];
    constexpr double eps = 1e-6;
    if (startEdge)
    {
        double lo = 0.0;
        const bool touchingPrev = slice > 0 && std::abs (manualSlices[(size_t) slice - 1].end - s.start) < eps;
        if (link && touchingPrev) lo = manualSlices[(size_t) slice - 1].start + minLen;
        const double v = juce::jlimit (lo, s.end - minLen, fraction);
        if (link && touchingPrev) manualSlices[(size_t) slice - 1].end = v;
        s.start = v;
    }
    else
    {
        double hi = 1.0;
        const bool touchingNext = slice + 1 < (int) manualSlices.size() && std::abs (manualSlices[(size_t) slice + 1].start - s.end) < eps;
        if (link && touchingNext) hi = manualSlices[(size_t) slice + 1].end - minLen;
        const double v = juce::jlimit (s.start + minLen, hi, fraction);
        if (link && touchingNext) manualSlices[(size_t) slice + 1].start = v;
        s.end = v;
    }
    commitManual();
}

int NoviSliceProcessor::addSliceMarker (double fraction)
{
    ensureManual();
    if ((int) manualSlices.size() >= novi::maxSlices)
        return -1;
    if (apvts.getRawParameterValue ("zeroSnap")->load() > 0.5f)
        fraction = chopper.snapToZero (fraction);
    for (size_t i = 0; i < manualSlices.size(); ++i)
    {
        auto& s = manualSlices[i];
        if (fraction > s.start + 1e-4 && fraction < s.end - 1e-4)
        {
            const novi::Slice tail { fraction, s.end };
            s.end = fraction;
            manualSlices.insert (manualSlices.begin() + (long) i + 1, tail);
            commitManual();
            return (int) i + 1;
        }
    }
    // In a gap: a new slice up to the next slice (or the end of the trim).
    double end = apvts.getRawParameterValue ("trimEnd")->load();
    for (auto& s : manualSlices)
        if (s.start > fraction) { end = std::min (end, s.start); }
    if (end - fraction < 1e-4)
        return -1;
    manualSlices.push_back ({ fraction, end });
    commitManual();
    for (size_t i = 0; i < manualSlices.size(); ++i)
        if (std::abs (manualSlices[i].start - fraction) < 1e-9) return (int) i;
    return -1;
}

void NoviSliceProcessor::removeSliceMarker (int slice)
{
    ensureManual();
    if (slice <= 0 || slice >= (int) manualSlices.size())
        return;
    manualSlices[(size_t) slice - 1].end = std::max (manualSlices[(size_t) slice - 1].end, manualSlices[(size_t) slice].end);
    manualSlices.erase (manualSlices.begin() + slice);
    commitManual();
}

void NoviSliceProcessor::splitSlice (int slice)
{
    ensureManual();
    if (slice < 0 || slice >= (int) manualSlices.size() || (int) manualSlices.size() >= novi::maxSlices)
        return;
    auto& s = manualSlices[(size_t) slice];
    const double mid = 0.5 * (s.start + s.end);
    const novi::Slice tail { mid, s.end };
    s.end = mid;
    manualSlices.insert (manualSlices.begin() + slice + 1, tail);
    commitManual();
}

void NoviSliceProcessor::combineSlice (int slice) { removeSliceMarker (slice); }

void NoviSliceProcessor::clearSlices()
{
    ensureManual();
    auto f = [this] (const char* id) { return (double) apvts.getRawParameterValue (id)->load(); };
    manualSlices = { { f ("trimStart"), f ("trimEnd") } };
    commitManual();
}

void NoviSliceProcessor::setTrim (double a, double b)
{
    a = juce::jlimit (0.0, 1.0, a);
    b = juce::jlimit (0.0, 1.0, b);
    if (b - a < 0.002) return;
    setParam (apvts, "trimStart", (float) a);
    setParam (apvts, "trimEnd", (float) b);
    recomputeSlices();
}

// --- pads ------------------------------------------------------------------------------

novi::PadSettings NoviSliceProcessor::getPad (int key) const
{
    const juce::SpinLock::ScopedLockType sl (const_cast<juce::SpinLock&> (playbackLock));
    return pads[(size_t) juce::jlimit (0, novi::maxSlices - 1, key)];
}

void NoviSliceProcessor::setPad (int key, const novi::PadSettings& p)
{
    const juce::SpinLock::ScopedLockType sl (playbackLock);
    pads[(size_t) juce::jlimit (0, novi::maxSlices - 1, key)] = p;
}

void NoviSliceProcessor::clearPadFlags (bool mutes, bool solos)
{
    const juce::SpinLock::ScopedLockType sl (playbackLock);
    for (auto& p : pads)
    {
        if (mutes) p.mute = false;
        if (solos) p.solo = false;
    }
}

bool NoviSliceProcessor::anyPadSolo() const
{
    const juce::SpinLock::ScopedLockType sl (const_cast<juce::SpinLock&> (playbackLock));
    for (auto& p : pads)
        if (p.solo) return true;
    return false;
}

// --- shuffle -------------------------------------------------------------------------

void NoviSliceProcessor::shuffleOrder()
{
    const int total = juce::jmax (1, getNumSlices());
    std::vector<int> perm ((size_t) total);
    std::iota (perm.begin(), perm.end(), 0);
    auto& rnd = juce::Random::getSystemRandom();
    for (int i = total - 1; i > 0; --i)
        std::swap (perm[(size_t) i], perm[(size_t) rnd.nextInt (i + 1)]);
    for (int k = 0; k < novi::maxSlices; ++k)
        order[(size_t) k].store (k < total ? perm[(size_t) k] : k);
}

void NoviSliceProcessor::resetOrder()
{
    for (int k = 0; k < novi::maxSlices; ++k)
        order[(size_t) k].store (k);
}

bool NoviSliceProcessor::isShuffled() const
{
    for (int k = 0; k < novi::maxSlices; ++k)
        if (order[(size_t) k].load() != k)
            return true;
    return false;
}

int NoviSliceProcessor::getKeyForSlice (int slice) const
{
    for (int k = 0; k < novi::maxSlices; ++k)
        if (order[(size_t) k].load() == slice) return k;
    return slice;
}

// --- processing ------------------------------------------------------------------------

void NoviSliceProcessor::normalizeSample()
{
    {
        const juce::ScopedLock l (sampleInfoLock);
        const float peak = original.getMagnitude (0, original.getNumSamples());
        if (peak <= 1e-6f) return;
        original.applyGain (0.989f / peak);   // -0.1 dBFS
    }
    audioChanged (true);
}

void NoviSliceProcessor::reverseSample()
{
    {
        const juce::ScopedLock l (sampleInfoLock);
        if (original.getNumSamples() == 0) return;
        original.reverse (0, original.getNumSamples());
    }
    // Mirror the slice points and the trim so they still line up with the same audio.
    for (auto& s : manualSlices)
        s = { 1.0 - s.end, 1.0 - s.start };
    std::sort (manualSlices.begin(), manualSlices.end(), [] (const novi::Slice& a, const novi::Slice& b) { return a.start < b.start; });
    const double a = apvts.getRawParameterValue ("trimStart")->load(), b = apvts.getRawParameterValue ("trimEnd")->load();
    setParam (apvts, "trimStart", (float) (1.0 - b));
    setParam (apvts, "trimEnd", (float) (1.0 - a));
    audioChanged (true);
}

void NoviSliceProcessor::sampleToMono (int mode)
{
    {
        const juce::ScopedLock l (sampleInfoLock);
        const int len = original.getNumSamples();
        if (len == 0) return;
        auto* L = original.getWritePointer (0);
        auto* R = original.getWritePointer (1);
        for (int i = 0; i < len; ++i)
        {
            const float v = mode == 1 ? L[i] : mode == 2 ? R[i] : 0.5f * (L[i] + R[i]);
            L[i] = R[i] = v;
        }
    }
    audioChanged (true);
}

// --- export ----------------------------------------------------------------------------

static juce::String safeName (juce::String s)
{
    s = juce::File::createLegalFileName (s.trim());
    return s.isEmpty() ? juce::String ("NoviSlice") : s;
}

void NoviSliceProcessor::writeSliceWav (int slice, const juce::File& file) const
{
    const juce::ScopedLock l (sampleInfoLock);
    if (slice < 0 || slice >= uiMap.count || original.getNumSamples() == 0)
        return;
    const int len = original.getNumSamples();
    const int a = juce::jlimit (0, len - 1, (int) std::lround (uiMap.s[(size_t) slice].start * len));
    const int b = juce::jlimit (a + 1, len, (int) std::lround (uiMap.s[(size_t) slice].end * len));

    file.deleteFile();
    auto out = file.createOutputStream();
    if (out == nullptr) return;
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (out.get(), originalRate, 2, 24, {}, 0));
    if (writer == nullptr) return;
    out.release();
    writer->writeFromAudioSampleBuffer (original, a, b - a);
}

int NoviSliceProcessor::exportSlicesTo (const juce::File& folder)
{
    if (! folder.isDirectory() && ! folder.createDirectory())
        return 0;
    const auto name = safeName (getSampleName());
    const int count = getNumSlices();
    for (int i = 0; i < count; ++i)
        writeSliceWav (i, folder.getChildFile (name + " slice " + juce::String (i + 1).paddedLeft ('0', 2) + ".wav"));
    return count;
}

static juce::File tempFolder()
{
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("NoviSlice");
    dir.createDirectory();
    return dir;
}

juce::File NoviSliceProcessor::writeSliceToTempFile (int key)
{
    const int slice = getSliceForKey (key);
    auto f = tempFolder().getChildFile (safeName (getSampleName()) + " slice " + juce::String (slice + 1).paddedLeft ('0', 2) + ".wav");
    writeSliceWav (slice, f);
    return f;
}

juce::File NoviSliceProcessor::writeMidiPatternToTempFile()
{
    const auto map = getSliceMap();
    const double tempo = getLoopBpm() > 0.0 ? getLoopBpm() : 120.0;
    const double seconds = getSampleSeconds();
    const double origin = map.count > 0 ? map.s[0].start : 0.0;
    constexpr int ppq = 960;

    juce::MidiMessageSequence seq;
    seq.addEvent (juce::MidiMessage::textMetaEvent (3, getSampleName().isEmpty() ? "NoviSlice" : getSampleName()), 0);
    seq.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::lround (60000000.0 / tempo)), 0);
    seq.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0);
    auto toTicks = [&] (double fraction) { return (fraction - origin) * seconds * tempo / 60.0 * ppq; };
    for (int s = 0; s < map.count; ++s)
    {
        const int note = novi::noteForSliceKey (getKeyForSlice (s));
        if (note < 0) continue;
        const double on = toTicks (map.s[(size_t) s].start);
        const double off = juce::jmax (on + 10.0, toTicks (map.s[(size_t) s].end) - 4.0);
        seq.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), on);
        seq.addEvent (juce::MidiMessage::noteOff (1, note), off);
    }
    seq.updateMatchedPairs();

    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (ppq);
    mf.addTrack (seq);
    auto f = tempFolder().getChildFile (safeName (getSampleName()) + " pattern.mid");
    f.deleteFile();
    if (auto out = f.createOutputStream())
        mf.writeTo (*out);
    return f;
}

// --- state -----------------------------------------------------------------------------

void NoviSliceProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    juce::ValueTree sampleTree ("Sample");
    {
        const juce::ScopedLock l (sampleInfoLock);
        sampleTree.setProperty ("name", sampleName, nullptr);
        sampleTree.setProperty ("path", samplePath, nullptr);
        // The audio file itself is stored so projects still open if the file moves.
        if (fileData.getSize() > 0)
            sampleTree.setProperty ("data", fileData.toBase64Encoding(), nullptr);
    }
    juce::StringArray ord;
    for (int k = 0; k < novi::maxSlices; ++k)
        ord.add (juce::String (order[(size_t) k].load()));
    sampleTree.setProperty ("order", ord.joinIntoString (","), nullptr);

    juce::StringArray manual;
    for (auto& s : manualSlices)
        manual.add (juce::String (s.start, 9) + ":" + juce::String (s.end, 9));
    sampleTree.setProperty ("manual", manual.joinIntoString (","), nullptr);

    juce::ValueTree padTree ("Pads");
    for (int k = 0; k < novi::maxSlices; ++k)
    {
        const auto p = getPad (k);
        if (p.isDefault()) continue;
        juce::ValueTree t ("Pad");
        t.setProperty ("key", k, nullptr);
        t.setProperty ("tune", p.tune, nullptr);
        t.setProperty ("level", p.levelDb, nullptr);
        t.setProperty ("pan", p.pan, nullptr);
        t.setProperty ("cutoff", p.cutoffHz, nullptr);
        t.setProperty ("tail", p.tailMs, nullptr);
        t.setProperty ("reverse", p.reverse, nullptr);
        t.setProperty ("mute", p.mute, nullptr);
        t.setProperty ("solo", p.solo, nullptr);
        t.setProperty ("loop", p.loop, nullptr);
        t.setProperty ("playMode", p.playMode, nullptr);
        t.setProperty ("muteGroup", p.muteGroup, nullptr);
        t.setProperty ("repeats", p.repeats, nullptr);
        t.setProperty ("motion", p.motion, nullptr);
        t.setProperty ("cycles", p.cycles, nullptr);
        padTree.appendChild (t, nullptr);
    }

    for (auto name : { "Sample", "Pads" })
        state.removeChild (state.getChildWithName (name), nullptr);
    state.appendChild (sampleTree, nullptr);
    state.appendChild (padTree, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void NoviSliceProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr)
        return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.hasType (apvts.state.getType()))
        return;

    auto sampleTree = state.getChildWithName ("Sample");
    auto padTree = state.getChildWithName ("Pads");
    state.removeChild (sampleTree, nullptr);
    state.removeChild (padTree, nullptr);
    apvts.replaceState (state);

    novi::PadArray newPads {};
    for (auto t : padTree)
    {
        const int k = t["key"];
        if (k < 0 || k >= novi::maxSlices) continue;
        auto& p = newPads[(size_t) k];
        p.tune = t["tune"];
        p.levelDb = t["level"];
        p.pan = t["pan"];
        p.cutoffHz = t.getProperty ("cutoff", 20000.f);
        p.tailMs = t["tail"];
        p.reverse = t["reverse"];
        p.mute = t["mute"];
        p.solo = t["solo"];
        p.loop = t["loop"];
        p.playMode = t["playMode"];
        p.muteGroup = t["muteGroup"];
        p.repeats = t["repeats"];
        p.motion = t["motion"];
        p.cycles = t["cycles"];
    }
    {
        const juce::SpinLock::ScopedLockType sl (playbackLock);
        pads = newPads;
    }

    manualSlices.clear();
    if (sampleTree.isValid())
    {
        for (auto token : juce::StringArray::fromTokens (sampleTree["manual"].toString(), ",", ""))
        {
            const auto a = token.upToFirstOccurrenceOf (":", false, false).getDoubleValue();
            const auto b = token.fromFirstOccurrenceOf (":", false, false).getDoubleValue();
            if (b > a) manualSlices.push_back ({ a, b });
        }

        const juce::String name = sampleTree["name"], path = sampleTree["path"];
        juce::MemoryBlock block;
        bool loaded = false;
        if (block.fromBase64Encoding (sampleTree["data"].toString()) && block.getSize() > 0)
            loaded = loadFromMemory (block, name, path, false);
        if (! loaded && path.isNotEmpty())
        {
            const juce::File f (path);
            juce::MemoryBlock fileBlock;
            if (f.existsAsFile() && f.loadFileAsData (fileBlock))
                loadFromMemory (fileBlock, name, path, false);
        }

        auto tokens = juce::StringArray::fromTokens (sampleTree["order"].toString(), ",", "");
        for (int k = 0; k < novi::maxSlices; ++k)
            order[(size_t) k].store (k < tokens.size() ? juce::jlimit (0, novi::maxSlices - 1, tokens[k].getIntValue()) : k);
    }
    recomputeSlices();
}

juce::AudioProcessorEditor* NoviSliceProcessor::createEditor() { return new NoviSliceEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NoviSliceProcessor(); }
