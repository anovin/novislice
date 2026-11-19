#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

// NoviSlice's look: a vintage hardware sampler. Cream chassis, green LCD,
// rubber pads, black knurled knobs. Everything lives on a fixed design grid
// (designW x designH) and is scaled to the window.

struct NoviFonts
{
    NoviFonts();
    juce::Font bold (float h, float kern = 0.f) const;
    juce::Font semi (float h, float kern = 0.f) const;
    juce::Font mono (float h, float kern = 0.f) const;
    juce::Typeface::Ptr tfBold, tfSemi, tfMono;
};

class NoviLook : public juce::LookAndFeel_V4
{
public:
    explicit NoviLook (const NoviFonts& f);

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    juce::Font getLabelFont (juce::Label&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;

private:
    const NoviFonts& fonts;
};

class LcdDisplay : public juce::Component
{
public:
    LcdDisplay (NoviSliceProcessor&, const NoviFonts&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void refresh();

    bool dropHover = false;

private:
    NoviSliceProcessor& proc;
    const NoviFonts& fonts;
    std::vector<float> peaks;
    juce::String name;
    int lastPeakVersion = -1;
    int auditionNote = -1;
};

class PadButton : public juce::Component
{
public:
    PadButton (NoviSliceProcessor&, const NoviFonts&);
    void setKey (int keyIndex, bool enabled);
    void setLit (bool lit);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    NoviSliceProcessor& proc;
    const NoviFonts& fonts;
    int key = 0;
    bool enabled = true, lit = false, pressed = false;
};

class PerfButton : public juce::Component
{
public:
    PerfButton (NoviSliceProcessor&, const NoviFonts&, int fx);
    void setLit (bool lit);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    NoviSliceProcessor& proc;
    const NoviFonts& fonts;
    int fx;
    bool lit = false, pressed = false, latched = false;
};

class FacePanel : public juce::Component
{
public:
    explicit FacePanel (NoviSliceProcessor&);
    ~FacePanel() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void refresh();

    static constexpr int designW = 1100, designH = 736;
    static constexpr int meterY = 598;

    LcdDisplay& getLcd() { return lcd; }

private:
    struct Knob
    {
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
        juce::String label;
    };
    struct Choice
    {
        std::unique_ptr<juce::ComboBox> box;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
        juce::String label;
    };
    struct Toggle
    {
        std::unique_ptr<juce::TextButton> button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
    };
    struct SectionBox { juce::String title; juce::Rectangle<int> area; };

    Knob& addKnob (const juce::String& paramId, const juce::String& label);
    Choice& addChoice (const juce::String& paramId, const juce::String& label, const juce::StringArray& items);
    Toggle& addToggle (const juce::String& paramId, const juce::String& text);
    void placeKnob (const juce::String& paramId, int x, int y);
    void placeChoice (const juce::String& paramId, int x, int y, int w);
    void loadViaChooser();
    void paintChassis (juce::Graphics&);

    NoviSliceProcessor& proc;
    NoviFonts fonts;
    NoviLook look { fonts };

    LcdDisplay lcd { proc, fonts };
    juce::TextButton loadButton { "LOAD" }, shuffleButton { "SHUFFLE" }, resetButton { "RESET" };
    juce::TextButton bankA { "A" }, bankB { "B" };
    std::map<juce::String, Knob> knobs;
    std::map<juce::String, Choice> choices;
    std::map<juce::String, Toggle> toggles;
    std::array<std::unique_ptr<PadButton>, 16> pads;
    std::array<std::unique_ptr<PerfButton>, novi::numPerfFx> perfButtons;
    std::vector<SectionBox> sections;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Image chassisCache;

    int bank = 0;
    float meterLevel = 0.f;
};

class NoviSliceEditor : public juce::AudioProcessorEditor,
                        public juce::FileDragAndDropTarget,
                        private juce::Timer
{
public:
    explicit NoviSliceEditor (NoviSliceProcessor&);
    ~NoviSliceEditor() override;

    void paint (juce::Graphics&) override {}
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override;
    void fileDragExit (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    void timerCallback() override;

    NoviSliceProcessor& proc;
    FacePanel panel;
};
