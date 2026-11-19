#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

const juce::StringArray NoviSliceProcessor::barsChoices   { "1", "2", "3", "4", "5", "6", "7", "8", "16" };
const juce::StringArray NoviSliceProcessor::slicesChoices { "1", "2", "4", "8" };
const juce::StringArray NoviSliceProcessor::lfoChoices    { "OFF", "1/1", "1/2", "1/4", "1/8", "1/16" };
const juce::StringArray NoviSliceProcessor::modeChoices   { "LP", "HP", "BP" };
const juce::StringArray NoviSliceProcessor::delayChoices  { "1/16", "1/8", "1/8D", "1/4", "1/4D", "1/2" };

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
    l.add (floatParam ("texSize", "Grain size", skewed (20.f, 250.f, 90.f, 0.1f), 90.f, msPlain));
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
    return l;
}

NoviSliceProcessor::NoviSliceProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "NoviSlice", createLayout())
{
    formats.registerBasicFormats();
    for (int i = 0; i < novi::maxSlices; ++i)
        order[(size_t) i].store (i);
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

    std::array<int, novi::maxSlices> ord {};
    for (int k = 0; k < novi::maxSlices; ++k)
        ord[(size_t) k] = order[(size_t) k].load();
    engine.setOrder (ord);

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

bool NoviSliceProcessor::loadFromMemory (const juce::MemoryBlock& data, const juce::String& name,
                                         const juce::String& path, bool guessBars)
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

    // UI peaks
    std::vector<float> newPeaks ((size_t) numPeakColumns * 2, 0.f);
    for (int c = 0; c < numPeakColumns; ++c)
    {
        const int a = (int) ((juce::int64) c * len / numPeakColumns);
        const int b = juce::jmax (a + 1, (int) ((juce::int64) (c + 1) * len / numPeakColumns));
        float lo = 0.f, hi = 0.f;
        for (int i = a; i < b && i < len; ++i)
        {
            const float s = 0.5f * (buf.getSample (0, i) + buf.getSample (1, i));
            lo = juce::jmin (lo, s);
            hi = juce::jmax (hi, s);
        }
        newPeaks[(size_t) c * 2] = lo;
        newPeaks[(size_t) c * 2 + 1] = hi;
    }

    {
        const juce::ScopedLock l (sampleInfoLock);
        original = std::move (buf);
        originalRate = reader->sampleRate;
        fileData = data;
        sampleName = name;
        samplePath = path;
        peaks = std::move (newPeaks);
    }
    ++sampleVersion;

    if (guessBars)
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
        if (auto* bars = apvts.getParameter ("bars"))
            bars->setValueNotifyingHost (bars->convertTo0to1 ((float) best));
        // A BPM in the file name becomes the Loop BPM; otherwise go back to auto.
        setLoopBpm (wanted > 0.0 ? wanted : 0.0);
        resetOrder();
    }

    rebuildPlaybackData();
    return true;
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

juce::String NoviSliceProcessor::getSampleName() const { const juce::ScopedLock l (sampleInfoLock); return sampleName; }
bool NoviSliceProcessor::hasSample() const             { const juce::ScopedLock l (sampleInfoLock); return original.getNumSamples() > 0; }
std::vector<float> NoviSliceProcessor::getPeaks() const { const juce::ScopedLock l (sampleInfoLock); return peaks; }

double NoviSliceProcessor::getLoopBpm() const
{
    novi::EngineParams p;
    readParams (p);
    const juce::ScopedLock l (sampleInfoLock);
    if (original.getNumSamples() < 2) return p.sampleBpm >= novi::minSampleBpm ? (double) p.sampleBpm : 0.0;
    return novi::SliceEngine::tempoOf (p, original.getNumSamples(), originalRate);
}

int NoviSliceProcessor::getNumSlices() const
{
    novi::EngineParams p;
    readParams (p);
    const juce::ScopedLock l (sampleInfoLock);
    return novi::SliceEngine::sliceCount (p, original.getNumSamples(), originalRate);
}

double NoviSliceProcessor::getSliceFraction() const
{
    novi::EngineParams p;
    readParams (p);
    const juce::ScopedLock l (sampleInfoLock);
    const double len = original.getNumSamples();
    if (len < 2) return 1.0 / novi::SliceEngine::sliceCount (p, 0, originalRate);
    return novi::SliceEngine::sliceLengthOf (p, len, originalRate) / len;
}

bool NoviSliceProcessor::isLoopBpmManual() const
{
    return apvts.getRawParameterValue ("sampleBpm")->load() >= novi::minSampleBpm;
}

void NoviSliceProcessor::setLoopBpm (double bpm)
{
    if (auto* p = apvts.getParameter ("sampleBpm"))
    {
        const float v = bpm >= novi::minSampleBpm ? (float) juce::jlimit ((double) novi::minSampleBpm, (double) novi::maxSampleBpm, bpm) : 0.f;
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (v));
        p->endChangeGesture();
    }
}

void NoviSliceProcessor::shuffleOrder()
{
    const int total = getNumSlices();
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

// --- state -----------------------------------------------------------------------

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

    state.removeChild (state.getChildWithName ("Sample"), nullptr);
    state.appendChild (sampleTree, nullptr);

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
    state.removeChild (sampleTree, nullptr);
    apvts.replaceState (state);

    if (sampleTree.isValid())
    {
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
}

juce::AudioProcessorEditor* NoviSliceProcessor::createEditor() { return new NoviSliceEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NoviSliceProcessor(); }
