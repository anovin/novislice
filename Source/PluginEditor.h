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

enum Page { pageSound = 0, pageChop, pagePad, pagePlay, numPages };

class LcdDisplay : public juce::Component
{
public:
    LcdDisplay (NoviSliceProcessor&, const NoviFonts&);
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void refresh();
    void flash (const juce::String& message);

    bool dropHover = false;
    bool editing = false;                 // CHOP page: markers can be moved
    int  selectedSlice = -1;
    std::function<void (int slice)> onSliceClicked;

private:
    juce::Rectangle<float> waveArea() const;
    double toFraction (float x) const;
    float toX (double fraction) const;
    enum class Grab { none, sliceStart, sliceEnd, trimStart, trimEnd };
    Grab hitTest (juce::Point<float> p, int& slice) const;

    NoviSliceProcessor& proc;
    const NoviFonts& fonts;
    std::vector<float> peaks;
    novi::SliceMap map;
    juce::String name, message;
    juce::uint32 messageUntil = 0;
    int lastPeakVersion = -1, lastSliceVersion = -1;
    int auditionNote = -1;
    Grab grab = Grab::none;
    int grabSlice = -1;
};

// Loop BPM: an LCD readout with up/down arrows. Drag to change, double-click to type.
class BpmBox : public juce::Component, public juce::SettableTooltipClient
{
public:
    BpmBox (NoviSliceProcessor&, const NoviFonts&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    double startingTempo() const;
    void set (double bpm);
    void commitText();
    juce::Rectangle<float> arrowArea() const;

    NoviSliceProcessor& proc;
    const NoviFonts& fonts;
    juce::ParameterAttachment attachment;
    juce::TextEditor editor;
    float value = 0.f;
    double dragStart = 0.0;
    bool dragging = false;
};

class PadButton : public juce::Component
{
public:
    PadButton (NoviSliceProcessor&, const NoviFonts&);
    void setState (int keyIndex, bool enabled, bool lit, bool selected, int sixteenLevel);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    std::function<void (int key)> onSelect;

private:
    void release();

    NoviSliceProcessor& proc;
    const NoviFonts& fonts;
    int key = 0, level = -1;
    bool enabled = true, lit = false, selected = false, pressed = false, draggedOut = false;
    novi::PadSettings shown;
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

// A button you drag out of the plugin to drop a file into FL Studio.
class DragFileButton : public juce::TextButton
{
public:
    DragFileButton (const juce::String& text, std::function<juce::File()> make) : juce::TextButton (text), makeFile (std::move (make)) {}
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

private:
    std::function<juce::File()> makeFile;
    bool dragging = false;
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
    void setPage (int page);
    void selectPad (int key);

    static constexpr int designW = 1100, designH = 772;
    static constexpr int meterY = 642;

    LcdDisplay& getLcd() { return lcd; }

private:
    struct Labelled { juce::Component* comp; juce::String label; int page; };
    struct SectionBox { juce::String title; juce::Rectangle<int> area; int page; };

    juce::Slider& addKnob (const juce::String& paramId, const juce::String& label, int page);
    juce::ComboBox& addChoice (const juce::String& paramId, const juce::String& label, const juce::StringArray& items, int page);
    juce::TextButton& addToggle (const juce::String& paramId, const juce::String& text, int page);
    juce::Slider& addPadKnob (const juce::String& label, double lo, double hi, double step, double centre, double def,
                              std::function<juce::String (double)> text);
    juce::ComboBox& addPadChoice (const juce::String& label, const juce::StringArray& items);
    juce::TextButton& addPadToggle (const juce::String& text);
    juce::TextButton& addButton (const juce::String& text, int page, std::function<void()> onClick);
    void place (juce::Component& c, int x, int y, int w, int h);
    void placeKnob (juce::Component& c, int x, int y) { place (c, x, y + 16, 62, 80); }
    void placeLabelled (juce::Component& c, int x, int y, int w, int h = 26) { place (c, x, y + 16, w, h); }
    void addToPage (juce::Component& c, int page, const juce::String& label = {});
    void loadPadControls();
    void storePadControls();
    void loadViaChooser();
    void exportWavs();
    void paintChassis (juce::Graphics&);

    NoviSliceProcessor& proc;
    NoviFonts fonts;
    NoviLook look { fonts };

    LcdDisplay lcd { proc, fonts };
    BpmBox bpmBox { proc, fonts };
    juce::TooltipWindow tooltips { this, 600 };
    juce::TextButton playButton { "PLAY" };
    juce::uint32 playPressedAt = 0;
    juce::TextButton loadButton { "LOAD" }, shuffleButton { "SHUFFLE" }, resetButton { "RESET" };
    juce::TextButton bankA { "A" }, bankB { "B" };
    std::array<juce::TextButton, numPages> pageTabs;

    std::vector<std::unique_ptr<juce::Slider>> sliders;
    std::vector<std::unique_ptr<juce::ComboBox>> combos;
    std::vector<std::unique_ptr<juce::Button>> buttons;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;
    std::map<juce::String, juce::Component*> byId;

    // pad edit controls
    juce::Slider *padTune = nullptr, *padLevel = nullptr, *padPan = nullptr, *padCutoff = nullptr, *padTail = nullptr;
    juce::ComboBox *padPlayMode = nullptr, *padMuteGroup = nullptr, *padRepeats = nullptr, *padMotion = nullptr, *padCycles = nullptr;
    juce::TextButton *padReverse = nullptr, *padLoop = nullptr, *padMute = nullptr, *padSolo = nullptr;
    bool loadingPad = false;

    std::unique_ptr<DragFileButton> dragMidi;
    std::array<std::unique_ptr<PadButton>, 16> pads;
    std::array<std::unique_ptr<PerfButton>, novi::numPerfFx> perfButtons;
    std::vector<Labelled> labelled;
    std::vector<SectionBox> sections;
    std::vector<std::pair<juce::Component*, int>> pageMembers;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Image chassisCache;

    int page = pageSound;
    int bank = 0;
    int selectedPad = 0;
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

    FacePanel& getPanel() { return panel; }

private:
    void timerCallback() override;

    NoviSliceProcessor& proc;
    FacePanel panel;
};
