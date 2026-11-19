#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "SliceEngine.h"

class NoviSliceProcessor : public juce::AudioProcessor
{
public:
    NoviSliceProcessor();
    ~NoviSliceProcessor() override = default;

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

    // --- used by the editor (message thread) ---
    bool loadFile (const juce::File& file);
    juce::String getSampleName() const;
    bool hasSample() const;
    std::vector<float> getPeaks() const;          // interleaved min/max, numPeakColumns pairs
    double getLoopBpm() const;                    // Loop BPM if set, otherwise from length and BARS
    int getNumSlices() const;
    double getSliceFraction() const;              // one slice as a fraction of the loop
    bool isLoopBpmManual() const;
    void setLoopBpm (double bpm);                 // below minSampleBpm = auto
    std::atomic<double> hostBpm { 0.0 };          // last tempo reported by the DAW
    void readParams (novi::EngineParams& p) const;
    void shuffleOrder();
    void resetOrder();
    bool isShuffled() const;
    int getSliceForKey (int key) const { return order[(size_t) juce::jlimit (0, novi::maxSlices - 1, key)].load(); }

    static constexpr int numPeakColumns = 1200;
    std::atomic<int> sampleVersion { 0 };        // bumps when a new sample loads
    std::atomic<float> outputPeak { 0.f };

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;       // pads and PERFORM buttons play through this
    novi::SliceEngine engine;

    static const juce::StringArray barsChoices, slicesChoices, lfoChoices, modeChoices, delayChoices;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    bool loadFromMemory (const juce::MemoryBlock& data, const juce::String& name, const juce::String& path, bool guessBars);
    void rebuildPlaybackData();                   // resample the original to the current rate

    juce::AudioFormatManager formats;

    // Original audio as loaded
    juce::AudioBuffer<float> original;
    double originalRate = 44100.0;
    juce::MemoryBlock fileData;
    juce::String sampleName, samplePath;
    mutable juce::CriticalSection sampleInfoLock;
    std::vector<float> peaks;

    // Playback copy at the engine rate, swapped under the lock
    std::unique_ptr<novi::SampleData> playback;
    juce::SpinLock playbackLock;

    std::array<std::atomic<int>, novi::maxSlices> order;

    double currentRate = 44100.0;
    int    currentBlock = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoviSliceProcessor)
};
