// Renders the NoviSlice editor to a PNG: RenderUI <loop.wav> <out.png> [playing]
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 3) { std::printf ("usage: RenderUI loop.wav out.png [playing]\n"); return 1; }

    NoviSliceProcessor proc;
    proc.setPlayConfigDetails (0, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);
    if (juce::String (argv[1]).isNotEmpty() && ! proc.loadFile (juce::File (juce::String (argv[1]))))
        std::printf ("could not load %s\n", argv[1]);

    if (argc > 3)
    {
        // Play slice 6 with two effects held so the active states show.
        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, novi::noteForSliceKey (5), 1.f), 0);
        midi.addEvent (juce::MidiMessage::noteOn (1, novi::firstPerfKey + novi::fxRoll8, 1.f), 0);
        midi.addEvent (juce::MidiMessage::noteOn (1, novi::firstPerfKey + novi::fxThrow, 1.f), 0);
        for (int i = 0; i < 8; ++i) { proc.processBlock (buf, midi); midi.clear(); }
        if (auto* p = proc.apvts.getParameter ("sync")) p->setValueNotifyingHost (1.f);
        if (auto* p = proc.apvts.getParameter ("vinyl")) p->setValueNotifyingHost (0.3f);
        if (auto* p = proc.apvts.getParameter ("pitch")) p->setValueNotifyingHost (p->convertTo0to1 (4.f));
        if (auto* p = proc.apvts.getParameter ("dlyMix")) p->setValueNotifyingHost (0.25f);
        if (auto* p = proc.apvts.getParameter ("cutoff")) p->setValueNotifyingHost (p->convertTo0to1 (6000.f));
        proc.outputPeak = 0.5f;
    }

    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setSize (FacePanel::designW, FacePanel::designH);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (300);

    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.5f);
    const juce::File out { juce::String (argv[2]) };
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat png;
    png.writeImageToStream (img, os);
    std::printf ("wrote %s (%dx%d)\n", argv[2], img.getWidth(), img.getHeight());
    ed.reset();
    return 0;
}
