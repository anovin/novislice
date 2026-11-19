#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "Chopper.h"
#include "SliceEngine.h"

class NoviSliceProcessor : public juce::AudioProcessor,
                           private juce::Timer
{
public:
    NoviSliceProcessor();
    ~NoviSliceProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- sample (message thread) ----
    bool loadFile (const juce::File& file);
    juce::String getSampleName() const;
    bool hasSample() const;
    double getSampleSeconds() const;
    std::vector<float> getPeaks() const;          // interleaved min/max, numPeakColumns pairs

    // ---- slices (message thread) ----
    novi::SliceMap getSliceMap() const;
    int getNumSlices() const;
    double getLoopBpm() const;                    // the loop's tempo (Loop BPM if set, otherwise from BARS)
    bool isLoopBpmManual() const;
    void setLoopBpm (double bpm);                 // below minSampleBpm = auto
    int getChopMode() const;

    // Manual slice editing. Any edit switches the chop mode to MANUAL, starting from the current slices.
    void moveSliceEdge (int slice, bool startEdge, double fraction);
    int  addSliceMarker (double fraction);        // returns the new slice's index
    void removeSliceMarker (int slice);           // merges the slice into the one before it
    void splitSlice (int slice);
    void combineSlice (int slice);                // with the slice before it
    void clearSlices();
    void setTrim (double startFraction, double endFraction);
    void recomputeSlices();

    // ---- pads (message thread) ----
    novi::PadSettings getPad (int key) const;
    void setPad (int key, const novi::PadSettings& p);
    void clearPadFlags (bool mutes, bool solos);
    bool anyPadSolo() const;

    // ---- shuffle ----
    void shuffleOrder();
    void resetOrder();
    bool isShuffled() const;
    int getSliceForKey (int key) const { return order[(size_t) juce::jlimit (0, novi::maxSlices - 1, key)].load(); }
    int getKeyForSlice (int slice) const;

    // ---- processing and export ----
    void normalizeSample();
    void reverseSample();
    void sampleToMono (int mode);                 // 0 sum, 1 left, 2 right
    int  exportSlicesTo (const juce::File& folder);   // returns how many files were written
    juce::File writeSliceToTempFile (int key);
    juce::File writeMidiPatternToTempFile();

    void readParams (novi::EngineParams& p) const;

    // PLAY button
    void requestPreview (bool start) { previewRequest.store (start ? 1 : 2); }
    std::atomic<int> previewRequest { 0 };

    static constexpr int numPeakColumns = 1200;
    std::atomic<int> sampleVersion { 0 };         // bumps when the audio changes
    std::atomic<int> sliceVersion { 0 };          // bumps when the slices change
    std::atomic<float> outputPeak { 0.f };
    std::atomic<double> hostBpm { 0.0 };          // last tempo reported by the DAW

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;       // pads and PERFORM buttons play through this
    novi::SliceEngine engine;

    static const juce::StringArray barsChoices, slicesChoices, lfoChoices, modeChoices, delayChoices,
                                   chopChoices, polyChoices, repeatChoices, sixteenChoices;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    bool loadFromMemory (const juce::MemoryBlock& data, const juce::String& name, const juce::String& path, bool fresh);
    void audioChanged (bool encodeFile);          // after loading or processing: peaks, analysis, playback, slices
    void rebuildPlaybackData();
    novi::ChopSettings currentChopSettings() const;
    void ensureManual();
    void commitManual();
    void timerCallback() override;
    juce::String chopHash() const;
    void writeSliceWav (int slice, const juce::File& file) const;

    juce::AudioFormatManager formats;

    // Original audio as loaded (or processed)
    juce::AudioBuffer<float> original;
    double originalRate = 44100.0;
    juce::MemoryBlock fileData;
    juce::String sampleName, samplePath;
    mutable juce::CriticalSection sampleInfoLock;
    std::vector<float> peaks;
    novi::Chopper chopper;

    // Slices: what the UI sees, and what the engine plays
    novi::SliceMap uiMap, engineMap;
    std::vector<novi::Slice> manualSlices;
    std::atomic<double> loopTempo { 0.0 };
    juce::String lastChopHash;

    // Pads: edited on the message thread under playbackLock, read by the engine
    novi::PadArray pads {};

    // Playback copy at the engine rate, swapped under the lock
    std::unique_ptr<novi::SampleData> playback;
    juce::SpinLock playbackLock;

    std::array<std::atomic<int>, novi::maxSlices> order;

    double currentRate = 44100.0;
    int    currentBlock = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoviSliceProcessor)
};
