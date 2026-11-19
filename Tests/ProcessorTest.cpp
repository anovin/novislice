// Exercises the processor's slice editing, pads, state and export: built with -DNOVISLICE_SCREENSHOT=ON
#include "../Source/PluginProcessor.h"

static int failures = 0, checks = 0;
#define CHECK(cond, msg) do { ++checks; if (! (cond)) { ++failures; std::printf ("FAIL: %s (line %d)\n", msg, __LINE__); } } while (0)

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 2) { std::printf ("usage: ProcessorTest loop.wav\n"); return 1; }

    NoviSliceProcessor proc;
    proc.setPlayConfigDetails (0, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);
    CHECK (proc.loadFile (juce::File (juce::String (argv[1]))), "loads the loop");
    CHECK (std::abs (proc.getLoopBpm() - 90.0) < 0.01, "BPM from the file name");
    CHECK (proc.getNumSlices() == 8, "2 bars x 4 at 90 BPM");

    // manual editing
    const auto before = proc.getSliceMap();
    proc.moveSliceEdge (3, true, before.s[3].start + 0.01);
    auto m = proc.getSliceMap();
    CHECK (proc.getChopMode() == novi::chopManual, "editing switches to MANUAL");
    CHECK (std::abs (m.s[3].start - (before.s[3].start + 0.01)) < 1e-6, "marker moved");
    CHECK (std::abs (m.s[2].end - m.s[3].start) < 1e-9, "LINK moved the previous slice's end too");
    const int added = proc.addSliceMarker (0.5 * (m.s[5].start + m.s[5].end));
    CHECK (added == 6 && proc.getNumSlices() == 9, "double-click adds a marker");
    proc.splitSlice (0);
    CHECK (proc.getNumSlices() == 10, "split");
    proc.combineSlice (1);
    CHECK (proc.getNumSlices() == 9, "combine");
    proc.removeSliceMarker (6);
    CHECK (proc.getNumSlices() == 8, "alt-click removes a marker");
    proc.setTrim (0.25, 0.75);
    m = proc.getSliceMap();
    CHECK (m.s[0].start >= 0.25 - 1e-9 && m.s[m.count - 1].end <= 0.75 + 1e-9, "trim clips the manual slices");
    proc.clearSlices();
    CHECK (proc.getNumSlices() == 1, "clear leaves one slice");

    // back to regions, then transient
    if (auto* p = proc.apvts.getParameter ("chopMode")) p->setValueNotifyingHost (p->convertTo0to1 ((float) novi::chopRegions));
    proc.setTrim (0.0, 1.0);
    proc.recomputeSlices();
    CHECK (proc.getNumSlices() == 16, "regions mode: 16");
    if (auto* p = proc.apvts.getParameter ("chopMode")) p->setValueNotifyingHost (p->convertTo0to1 ((float) novi::chopTransient));
    proc.recomputeSlices();
    std::printf ("transient slices found: %d\n", proc.getNumSlices());
    CHECK (proc.getNumSlices() >= 8, "transient mode finds the drum hits");

    // Changing BARS / PER BAR while in another chop mode switches to the beat grid and re-chops
    if (auto* p = proc.apvts.getParameter ("chopMode")) p->setValueNotifyingHost (p->convertTo0to1 ((float) novi::chopTransient));
    proc.recomputeSlices();
    juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
    if (auto* p = proc.apvts.getParameter ("slices")) p->setValueNotifyingHost (p->convertTo0to1 (3.f));   // 8 per bar
    juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
    CHECK (proc.getChopMode() == novi::chopGrid, "PER BAR switches the chop mode to BPM GRID");
    CHECK (proc.getNumSlices() == 16, "2 bars x 8 per bar = 16 slices");
    if (auto* p = proc.apvts.getParameter ("bars")) p->setValueNotifyingHost (p->convertTo0to1 ((float) NoviSliceProcessor::barsChoices.indexOf ("1")));
    juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
    CHECK (proc.getNumSlices() == 8, "1 bar x 8 per bar = 8 slices");
    if (auto* p = proc.apvts.getParameter ("slices")) p->setValueNotifyingHost (p->convertTo0to1 (4.f));   // 16 per bar
    juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
    const auto gm = proc.getSliceMap();
    CHECK (proc.getNumSlices() == 16 && std::abs ((gm.s[1].start - gm.s[0].start) * proc.getSampleSeconds() - 60.0 / 90.0 / 4.0) < 1e-3,
           "16 per bar: slices every 16th note at 90 BPM");
    if (auto* p = proc.apvts.getParameter ("bars")) p->setValueNotifyingHost (p->convertTo0to1 ((float) NoviSliceProcessor::barsChoices.indexOf ("2")));
    if (auto* p = proc.apvts.getParameter ("slices")) p->setValueNotifyingHost (p->convertTo0to1 (2.f));
    juce::MessageManager::getInstance()->runDispatchLoopUntil (300);

    // pads + state round trip
    novi::PadSettings pd; pd.tune = 3.5f; pd.mute = true; pd.playMode = novi::playOneShot; pd.motion = novi::motionNext; pd.cycles = 4;
    proc.setPad (5, pd);
    proc.moveSliceEdge (2, true, proc.getSliceMap().s[2].start + 0.005);
    const int countBefore = proc.getNumSlices();
    juce::MemoryBlock state;
    proc.getStateInformation (state);
    NoviSliceProcessor other;
    other.setPlayConfigDetails (0, 2, 48000.0, 512);
    other.prepareToPlay (48000.0, 512);
    other.setStateInformation (state.getData(), (int) state.getSize());
    const auto op = other.getPad (5);
    CHECK (std::abs (op.tune - 3.5f) < 1e-6 && op.mute && op.playMode == novi::playOneShot && op.motion == novi::motionNext && op.cycles == 4, "pad settings saved");
    CHECK (other.getNumSlices() == countBefore && other.getChopMode() == novi::chopManual, "manual slices saved");
    CHECK (std::abs (other.getSliceMap().s[2].start - proc.getSliceMap().s[2].start) < 1e-8, "slice positions saved");

    // processing
    proc.normalizeSample();
    CHECK (proc.hasSample(), "normalize keeps the sample");
    proc.sampleToMono (0);
    proc.reverseSample();
    juce::MemoryBlock state2;
    proc.getStateInformation (state2);
    NoviSliceProcessor third;
    third.prepareToPlay (48000.0, 512);
    third.setStateInformation (state2.getData(), (int) state2.getSize());
    CHECK (third.hasSample() && std::abs (third.getSampleSeconds() - proc.getSampleSeconds()) < 0.01, "processed audio is saved in the project");

    // export
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("novislice-export-test");
    dir.deleteRecursively();
    const int n = proc.exportSlicesTo (dir);
    CHECK (n == proc.getNumSlices() && dir.getNumberOfChildFiles (juce::File::findFiles, "*.wav") == n, "one WAV per slice");
    const auto wav = proc.writeSliceToTempFile (0);
    CHECK (wav.existsAsFile() && wav.getSize() > 1000, "pad drag-out writes a WAV");
    const auto mid = proc.writeMidiPatternToTempFile();
    juce::MidiFile mf;
    juce::FileInputStream in (mid);
    CHECK (mf.readFrom (in), "MIDI pattern reads back");
    int notes = 0;
    if (mf.getNumTracks() > 0)
        for (auto* ev : *mf.getTrack (0)) if (ev->message.isNoteOn()) ++notes;
    CHECK (notes == proc.getNumSlices(), "one MIDI note per slice");

    std::printf ("%d/%d checks passed\n", checks - failures, checks);
    return failures == 0 ? 0 : 1;
}
