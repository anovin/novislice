#include "PluginEditor.h"
#include "BinaryData.h"

using juce::Colour;
using juce::Rectangle;

namespace col
{
    const Colour chassisTop   { 0xffe6dfcf };
    const Colour chassisBot   { 0xffd2c8b2 };
    const Colour ink          { 0xff2a2724 };
    const Colour print        { 0xff5d564c };
    const Colour printLight   { 0xff8a8174 };
    const Colour accent       { 0xffe0592a };
    const Colour accentDark   { 0xffb3431c };
    const Colour cream        { 0xffefe8d8 };
    const Colour keyCap       { 0xff3a3834 };
    const Colour keyCapDark   { 0xff262421 };
    const Colour lcdTop       { 0xffb3c487 };
    const Colour lcdBot       { 0xff97ab6b };
    const Colour lcdInk       { 0xff1e2a15 };
    const Colour lcdDim       { 0xff6c7f4c };
    const Colour ledOn        { 0xffff4b26 };
    const Colour ledOff       { 0xff5a2e24 };
    const Colour padTop       { 0xffaaa69c };
    const Colour padBot       { 0xff8d897f };
    const Colour bezel        { 0xff2c2a27 };
}

static juce::String noteName (int note) { return juce::MidiMessage::getMidiNoteName (note, true, true, 5); }

// =====================================================================

NoviFonts::NoviFonts()
{
    tfBold = juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedBold_ttf, BinaryData::BarlowCondensedBold_ttfSize);
    tfSemi = juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedSemiBold_ttf, BinaryData::BarlowCondensedSemiBold_ttfSize);
    tfMono = juce::Typeface::createSystemTypefaceFor (BinaryData::ShareTechMonoRegular_ttf, BinaryData::ShareTechMonoRegular_ttfSize);
}

static juce::Font makeFont (const juce::Typeface::Ptr& tf, float h, float kern)
{
    return juce::Font (juce::FontOptions (tf)).withHeight (h).withExtraKerningFactor (kern);
}

juce::Font NoviFonts::bold (float h, float k) const { return makeFont (tfBold, h, k); }
juce::Font NoviFonts::semi (float h, float k) const { return makeFont (tfSemi, h, k); }
juce::Font NoviFonts::mono (float h, float k) const { return makeFont (tfMono, h, k); }

// =====================================================================

NoviLook::NoviLook (const NoviFonts& f) : fonts (f)
{
    setColour (juce::PopupMenu::backgroundColourId, col::bezel);
    setColour (juce::PopupMenu::textColourId, col::cream);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::accent);
    setColour (juce::PopupMenu::highlightedTextColourId, col::cream);
    setColour (juce::ComboBox::textColourId, col::lcdInk);
    setColour (juce::Slider::textBoxTextColourId, col::ink);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, col::ink);
}

void NoviLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&)
{
    const auto area = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float r = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f - 7.f;
    const auto c = area.getCentre();

    // printed scale
    g.setColour (col::print);
    for (int i = 0; i <= 10; ++i)
    {
        const float a = start + (end - start) * i / 10.f;
        const float len = (i == 0 || i == 5 || i == 10) ? 5.f : 3.f;
        const auto p1 = c.getPointOnCircumference (r + 2.f, a), p2 = c.getPointOnCircumference (r + 2.f + len, a);
        g.drawLine ({ p1, p2 }, 1.2f);
    }

    // shadow
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.fillEllipse (Rectangle<float> (r * 2.f, r * 2.f).withCentre (c.translated (1.5f, 3.f)));

    // knurled skirt
    const auto body = Rectangle<float> (r * 2.f, r * 2.f).withCentre (c);
    g.setGradientFill (juce::ColourGradient (Colour (0xff3d3b37), body.getX(), body.getY(), Colour (0xff141311), body.getRight(), body.getBottom(), false));
    g.fillEllipse (body);
    g.setColour (Colour (0xff57534d));
    for (int i = 0; i < 36; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * i / 36.f;
        g.drawLine ({ c.getPointOnCircumference (r * 0.82f, a), c.getPointOnCircumference (r * 0.98f, a) }, 0.8f);
    }

    // cap
    const auto cap = Rectangle<float> (r * 1.5f, r * 1.5f).withCentre (c);
    g.setGradientFill (juce::ColourGradient (Colour (0xff4a4743), cap.getX(), cap.getY(), Colour (0xff1e1d1b), cap.getRight(), cap.getBottom(), false));
    g.fillEllipse (cap);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawEllipse (cap.reduced (1.f), 1.f);

    // pointer
    const float a = start + pos * (end - start);
    g.setColour (col::cream);
    juce::Path p;
    p.startNewSubPath (c.getPointOnCircumference (r * 0.22f, a));
    p.lineTo (c.getPointOnCircumference (r * 0.92f, a));
    g.strokePath (p, juce::PathStrokeType (3.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Font NoviLook::getLabelFont (juce::Label& l)
{
    if (dynamic_cast<juce::Slider*> (l.getParentComponent()) != nullptr)
        return fonts.mono (13.f);
    return LookAndFeel_V4::getLabelFont (l);
}

juce::Label* NoviLook::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (fonts.mono (12.5f));
    l->setColour (juce::Label::textColourId, col::ink);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setJustificationType (juce::Justification::centred);
    l->setEditable (false, false, false);
    l->setInterceptsMouseClicks (false, false);
    return l;
}

void NoviLook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.f);
    const bool on = b.getToggleState();
    const bool pressedLook = down;

    // recess
    g.setColour (Colour (0xff1c1a18).withAlpha (0.55f));
    g.fillRoundedRectangle (r.translated (0.f, 1.5f), 4.f);

    auto cap = r.withTrimmedBottom (pressedLook ? 0.f : 2.f).translated (0.f, pressedLook ? 1.5f : 0.f);
    g.setGradientFill (juce::ColourGradient (over ? col::keyCap.brighter (0.12f) : col::keyCap, cap.getX(), cap.getY(),
                                             col::keyCapDark, cap.getX(), cap.getBottom(), false));
    g.fillRoundedRectangle (cap, 4.f);
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawLine (cap.getX() + 4.f, cap.getY() + 1.f, cap.getRight() - 4.f, cap.getY() + 1.f, 1.f);

    if (b.getClickingTogglesState() || b.getRadioGroupId() != 0)
    {
        const auto led = Rectangle<float> (14.f, 3.5f).withCentre ({ cap.getCentreX(), cap.getY() + 6.f });
        if (on)
        {
            g.setColour (col::ledOn.withAlpha (0.35f));
            g.fillRoundedRectangle (led.expanded (3.f, 2.f), 3.f);
        }
        g.setColour (on ? col::ledOn : col::ledOff);
        g.fillRoundedRectangle (led, 1.5f);
    }
}

void NoviLook::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool down)
{
    const bool hasLed = b.getClickingTogglesState() || b.getRadioGroupId() != 0;
    auto r = b.getLocalBounds().toFloat().reduced (2.f).withTrimmedBottom (2.f);
    if (hasLed) r.removeFromTop (8.f);
    if (down) r.translate (0.f, 1.5f);
    g.setColour (b.isEnabled() ? col::cream : col::cream.withAlpha (0.35f));
    g.setFont (fonts.semi (13.f, 0.06f));
    g.drawText (b.getButtonText(), r, juce::Justification::centred, false);
}

void NoviLook::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = Rectangle<float> ((float) w, (float) h).reduced (0.5f);
    g.setColour (col::bezel);
    g.fillRoundedRectangle (r, 4.f);
    auto glass = r.reduced (3.f);
    g.setGradientFill (juce::ColourGradient (col::lcdTop, 0.f, glass.getY(), col::lcdBot, 0.f, glass.getBottom(), false));
    g.fillRoundedRectangle (glass, 2.f);
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    g.drawLine (glass.getX() + 1.f, glass.getY() + 1.f, glass.getRight() - 1.f, glass.getY() + 1.f, 1.5f);

    juce::Path tri;
    const float cx = glass.getRight() - 7.f, cy = glass.getCentreY();
    tri.addTriangle (cx - 3.5f, cy - 2.f, cx + 3.5f, cy - 2.f, cx, cy + 2.5f);
    g.setColour (box.isEnabled() ? col::lcdInk : col::lcdDim);
    g.fillPath (tri);
}

juce::Font NoviLook::getComboBoxFont (juce::ComboBox&) { return fonts.mono (14.f); }

void NoviLook::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (3, 1, box.getWidth() - 14, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centred);
}

juce::Font NoviLook::getPopupMenuFont() { return fonts.mono (15.f); }

void NoviLook::drawPopupMenuBackground (juce::Graphics& g, int, int)
{
    g.fillAll (col::bezel);
}

// =====================================================================

LcdDisplay::LcdDisplay (NoviSliceProcessor& p, const NoviFonts& f) : proc (p), fonts (f) {}

void LcdDisplay::refresh()
{
    const int v = proc.sampleVersion.load();
    if (v != lastPeakVersion)
    {
        lastPeakVersion = v;
        peaks = proc.getPeaks();
        name = proc.getSampleName();
    }
    repaint();
}

void LcdDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();

    // bezel
    g.setColour (col::bezel);
    g.fillRoundedRectangle (r, 9.f);
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawRoundedRectangle (r.reduced (1.f), 9.f, 1.f);

    auto glass = r.reduced (12.f);
    g.setGradientFill (juce::ColourGradient (col::lcdTop, 0.f, glass.getY(), col::lcdBot, 0.f, glass.getBottom(), false));
    g.fillRoundedRectangle (glass, 3.f);

    auto inner = glass.reduced (10.f, 8.f);
    auto status = inner.removeFromTop (20.f);
    auto numbers = inner.removeFromBottom (16.f);
    inner.removeFromBottom (2.f);
    auto wave = inner.reduced (0.f, 4.f);

    // status line
    const int total = proc.getNumSlices();
    const double bpm = proc.getLoopBpm();
    g.setColour (col::lcdInk);
    g.setFont (fonts.mono (15.f));
    juce::String left = bpm > 0.0 ? "BPM " + juce::String (bpm, 1) : juce::String ("BPM ---.-");
    left << "   " << juce::String (total).paddedLeft ('0', 2) << " SLICES";
    if (name.isNotEmpty())
        left << "   " << name.toUpperCase();
    auto flagsArea = status.removeFromRight (230.f);
    g.drawText (left, status, juce::Justification::centredLeft, true);

    auto flag = [&] (const juce::String& text, bool on)
    {
        auto f = flagsArea.removeFromRight (juce::GlyphArrangement::getStringWidth (fonts.mono (13.f), text) + 10.f);
        flagsArea.removeFromRight (4.f);
        f = f.reduced (0.f, 2.f);
        if (on)
        {
            g.setColour (col::lcdInk);
            g.fillRect (f);
            g.setColour (col::lcdTop);
        }
        else
            g.setColour (col::lcdDim);
        g.setFont (fonts.mono (13.f));
        g.drawText (text, f, juce::Justification::centred, false);
    };
    auto on = [this] (const char* id) { return proc.apvts.getRawParameterValue (id)->load() > 0.5f; };
    flag ("FULL", on ("full"));
    flag ("LOOP", on ("loop"));
    flag ("KEY", on ("keylock"));
    flag ("SYNC", on ("sync"));
    flag ("SHUF", proc.isShuffled());

    const bool loaded = ! peaks.empty() && proc.hasSample();
    if (! loaded)
    {
        g.setColour (col::lcdInk);
        g.setFont (fonts.mono (20.f, 0.05f));
        g.drawText ("DROP A LOOP HERE", wave.withTrimmedBottom (wave.getHeight() * 0.5f - 6.f), juce::Justification::centredBottom, false);
        g.setColour (col::lcdDim);
        g.setFont (fonts.mono (14.f));
        g.drawText ("WAV  AIFF  FLAC  MP3   -   OR PRESS LOAD", wave.withTrimmedTop (wave.getHeight() * 0.5f + 6.f), juce::Justification::centredTop, false);
    }
    else
    {
        const int activeSlice = proc.engine.uiActiveSlice.load();
        const float sliceW = wave.getWidth() / total;

        if (activeSlice >= 0 && activeSlice < total)
        {
            g.setColour (col::lcdInk);
            g.fillRect (Rectangle<float> (wave.getX() + activeSlice * sliceW, wave.getY() - 4.f, sliceW, wave.getHeight() + 8.f));
        }

        const int nPeaks = (int) peaks.size() / 2;
        const float pitch = 3.f;
        const int cols = (int) (wave.getWidth() / pitch);
        float maxAbs = 0.01f;
        for (float v : peaks) maxAbs = juce::jmax (maxAbs, std::abs (v));
        const float mid = wave.getCentreY(), half = wave.getHeight() * 0.5f;

        for (int c = 0; c < cols; ++c)
        {
            const int a = c * nPeaks / cols, b = juce::jmax (a + 1, (c + 1) * nPeaks / cols);
            float lo = 0.f, hi = 0.f;
            for (int i = a; i < b && i < nPeaks; ++i)
            {
                lo = juce::jmin (lo, peaks[(size_t) i * 2]);
                hi = juce::jmax (hi, peaks[(size_t) i * 2 + 1]);
            }
            const float x = wave.getX() + c * pitch;
            const int slice = juce::jlimit (0, total - 1, (int) ((x - wave.getX()) / sliceW));
            g.setColour (slice == activeSlice ? col::lcdTop : col::lcdInk);
            const float top = mid - (hi / maxAbs) * half, bottom = mid - (lo / maxAbs) * half;
            g.fillRect (x, std::floor (top), 2.f, juce::jmax (1.f, std::ceil (bottom - top)));
        }

        // slice dividers + numbers
        for (int s = 0; s < total; ++s)
        {
            const float x = wave.getX() + s * sliceW;
            if (s > 0)
            {
                g.setColour (s == activeSlice || s - 1 == activeSlice ? col::lcdTop : col::lcdInk.withAlpha (0.55f));
                for (float yy = wave.getY() - 4.f; yy < wave.getBottom() + 4.f; yy += 4.f)
                    g.fillRect (std::round (x), yy, 1.f, 2.f);
            }
            if (total <= 16 || s % 2 == 0)
            {
                g.setColour (s == activeSlice ? col::lcdInk : col::lcdDim);
                g.setFont (fonts.mono (12.f));
                g.drawText (juce::String (s + 1).paddedLeft ('0', 2), Rectangle<float> (x, numbers.getY(), sliceW * (total > 16 ? 2.f : 1.f), numbers.getHeight()),
                            juce::Justification::centred, false);
            }
        }

        const float ph = proc.engine.uiPlayhead.load();
        if (ph >= 0.f)
        {
            const float x = wave.getX() + ph * wave.getWidth();
            g.setColour (activeSlice >= 0 && juce::jlimit (0, total - 1, (int) (ph * total)) == activeSlice ? col::lcdTop : col::lcdInk);
            g.fillRect (x - 1.f, wave.getY() - 4.f, 2.f, wave.getHeight() + 8.f);
        }
    }

    // pixel grid and glass sheen
    g.setColour (col::lcdBot.withAlpha (0.18f));
    for (float yy = glass.getY() + 1.f; yy < glass.getBottom(); yy += 3.f)
        g.fillRect (glass.getX(), yy, glass.getWidth(), 1.f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.10f), glass.getX(), glass.getY(),
                                             juce::Colours::transparentWhite, glass.getX(), glass.getCentreY(), false));
    g.fillRoundedRectangle (glass, 3.f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawRoundedRectangle (glass, 3.f, 1.5f);

    if (dropHover)
    {
        g.setColour (col::lcdInk);
        const float dashes[] = { 6.f, 4.f };
        juce::Path outline;
        outline.addRoundedRectangle (glass.reduced (4.f), 3.f);
        juce::Path dashed;
        juce::PathStrokeType (2.f).createDashedStroke (dashed, outline, dashes, 2);
        g.fillPath (dashed);
    }
}

void LcdDisplay::mouseDown (const juce::MouseEvent& e)
{
    if (! proc.hasSample())
        return;
    const int total = proc.getNumSlices();
    auto wave = getLocalBounds().toFloat().reduced (22.f, 20.f);
    const int slice = juce::jlimit (0, total - 1, (int) ((e.position.x - wave.getX()) / wave.getWidth() * total));
    // Find the key that plays this slice (order may be shuffled).
    int key = slice;
    for (int k = 0; k < total; ++k)
        if (proc.getSliceForKey (k) == slice) { key = k; break; }
    auditionNote = novi::noteForSliceKey (key);
    if (auditionNote >= 0)
        proc.keyboardState.noteOn (1, auditionNote, 0.9f);
}

void LcdDisplay::mouseUp (const juce::MouseEvent&)
{
    if (auditionNote >= 0)
        proc.keyboardState.noteOff (1, auditionNote, 0.f);
    auditionNote = -1;
}

// =====================================================================

PadButton::PadButton (NoviSliceProcessor& p, const NoviFonts& f) : proc (p), fonts (f) {}

void PadButton::setKey (int k, bool en)
{
    if (k != key || en != enabled) { key = k; enabled = en; repaint(); }
}

void PadButton::setLit (bool l)
{
    if (l != lit) { lit = l; repaint(); }
}

void PadButton::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    // well
    g.setColour (Colour (0xff2b2926));
    g.fillRoundedRectangle (r, 9.f);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.drawRoundedRectangle (r.reduced (0.5f), 9.f, 1.f);

    auto pad = r.reduced (5.f).withTrimmedBottom (pressed ? 0.f : 2.f).translated (0.f, pressed ? 2.f : 0.f);
    const float alpha = enabled ? 1.f : 0.35f;
    Colour top = col::padTop, bot = col::padBot;
    if (lit) { top = Colour (0xffff8a52); bot = col::accent; }
    g.setGradientFill (juce::ColourGradient (top.withMultipliedAlpha (alpha), pad.getX(), pad.getY(),
                                             bot.withMultipliedAlpha (alpha), pad.getX(), pad.getBottom(), false));
    g.fillRoundedRectangle (pad, 7.f);
    if (lit)
    {
        g.setColour (col::accent.withAlpha (0.35f));
        g.drawRoundedRectangle (pad.expanded (2.f), 8.f, 3.f);
    }
    // subtle rubber texture
    g.setColour (juce::Colours::white.withAlpha (enabled ? 0.18f : 0.06f));
    g.drawLine (pad.getX() + 6.f, pad.getY() + 1.5f, pad.getRight() - 6.f, pad.getY() + 1.5f, 1.f);
    g.setColour (juce::Colours::black.withAlpha (0.15f));
    g.drawRoundedRectangle (pad, 7.f, 1.f);

    const auto ink = lit ? Colour (0xff3b160a) : col::ink.withAlpha (enabled ? 0.85f : 0.4f);
    g.setColour (ink);
    auto txt = pad.reduced (7.f, 5.f);
    g.setFont (fonts.bold (20.f));
    const int slice = proc.getSliceForKey (key);
    g.drawText (enabled ? juce::String (slice + 1).paddedLeft ('0', 2) : juce::String ("--"), txt, juce::Justification::topLeft, false);
    g.setFont (fonts.mono (11.f));
    const int note = novi::noteForSliceKey (key);
    if (note >= 0)
        g.drawText (noteName (note), txt, juce::Justification::bottomRight, false);
}

void PadButton::mouseDown (const juce::MouseEvent&)
{
    if (! enabled) return;
    pressed = true;
    repaint();
    if (const int note = novi::noteForSliceKey (key); note >= 0)
        proc.keyboardState.noteOn (1, note, 0.9f);
}

void PadButton::mouseUp (const juce::MouseEvent&)
{
    if (! pressed) return;
    pressed = false;
    repaint();
    if (const int note = novi::noteForSliceKey (key); note >= 0)
        proc.keyboardState.noteOff (1, note, 0.f);
}

// =====================================================================

PerfButton::PerfButton (NoviSliceProcessor& p, const NoviFonts& f, int index) : proc (p), fonts (f), fx (index) {}

void PerfButton::setLit (bool l)
{
    if (l != lit) { lit = l; repaint(); }
}

void PerfButton::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.f);
    g.setColour (Colour (0xff1c1a18).withAlpha (0.6f));
    g.fillRoundedRectangle (r.translated (0.f, 2.f), 5.f);

    auto cap = r.withTrimmedBottom (pressed ? 0.f : 3.f).translated (0.f, pressed ? 2.f : 0.f);
    g.setGradientFill (juce::ColourGradient (col::keyCap, cap.getX(), cap.getY(), col::keyCapDark, cap.getX(), cap.getBottom(), false));
    g.fillRoundedRectangle (cap, 5.f);
    g.setColour (juce::Colours::white.withAlpha (0.09f));
    g.drawLine (cap.getX() + 5.f, cap.getY() + 1.f, cap.getRight() - 5.f, cap.getY() + 1.f, 1.f);
    if (latched)
    {
        g.setColour (col::accent);
        g.drawRoundedRectangle (cap.reduced (1.f), 5.f, 1.5f);
    }

    const auto led = Rectangle<float> (7.f, 7.f).withPosition (cap.getX() + 7.f, cap.getY() + 7.f);
    if (lit)
    {
        g.setColour (col::ledOn.withAlpha (0.35f));
        g.fillEllipse (led.expanded (3.f));
    }
    g.setColour (lit ? col::ledOn : col::ledOff);
    g.fillEllipse (led);

    g.setColour (col::cream);
    g.setFont (fonts.semi (13.5f, 0.04f));
    g.drawText (novi::perfFxName (fx), cap.reduced (4.f, 0.f).withTrimmedTop (8.f).withTrimmedBottom (10.f), juce::Justification::centred, false);
    g.setColour (col::cream.withAlpha (0.45f));
    g.setFont (fonts.mono (10.f));
    g.drawText (noteName (novi::firstPerfKey + fx), cap.reduced (6.f, 4.f), juce::Justification::bottomRight, false);
}

void PerfButton::mouseDown (const juce::MouseEvent& e)
{
    const int note = novi::firstPerfKey + fx;
    pressed = true;
    if (e.mods.isShiftDown())
    {
        latched = ! latched;
        if (latched) proc.keyboardState.noteOn (1, note, 1.f);
        else         proc.keyboardState.noteOff (1, note, 0.f);
    }
    else if (! latched)
        proc.keyboardState.noteOn (1, note, 1.f);
    repaint();
}

void PerfButton::mouseUp (const juce::MouseEvent&)
{
    pressed = false;
    if (! latched)
        proc.keyboardState.noteOff (1, novi::firstPerfKey + fx, 0.f);
    repaint();
}

// =====================================================================

FacePanel::FacePanel (NoviSliceProcessor& p) : proc (p)
{
    setLookAndFeel (&look);
    setOpaque (true);

    addAndMakeVisible (lcd);

    for (auto* b : { &loadButton, &shuffleButton, &resetButton })
        addAndMakeVisible (*b);
    loadButton.onClick = [this] { loadViaChooser(); };
    shuffleButton.onClick = [this] { proc.shuffleOrder(); };
    resetButton.onClick = [this] { proc.resetOrder(); };

    for (auto* b : { &bankA, &bankB })
    {
        b->setRadioGroupId (77);
        b->setClickingTogglesState (true);
        addAndMakeVisible (*b);
    }
    bankA.setToggleState (true, juce::dontSendNotification);
    bankA.onClick = [this] { bank = 0; };
    bankB.onClick = [this] { bank = 1; };

    addChoice ("bars", "BARS", NoviSliceProcessor::barsChoices);
    addChoice ("slices", "SLICE/BAR", NoviSliceProcessor::slicesChoices);
    addToggle ("triplet", "TRIPLET");
    addToggle ("full", "FULL");
    addToggle ("loop", "LOOP");
    addToggle ("sync", "SYNC");
    addToggle ("keylock", "KEY LOCK");

    addKnob ("texAmount", "AMOUNT");
    addKnob ("texSize", "GRAIN");
    addKnob ("vinyl", "VINYL");
    addKnob ("pitch", "PITCH");
    addKnob ("glide", "GLIDE");
    addKnob ("xfade", "X-FADE");
    addKnob ("cutoff", "CUTOFF");
    addKnob ("res", "RESO");
    addKnob ("lfoDepth", "LFO DEPTH");
    addChoice ("mode", "MODE", NoviSliceProcessor::modeChoices);
    addChoice ("lfoRate", "LFO RATE", NoviSliceProcessor::lfoChoices);
    addKnob ("bits", "BITS");
    addKnob ("downsample", "RATE");
    addKnob ("dlyMix", "MIX");
    addKnob ("dlyFb", "FEEDBACK");
    addChoice ("dlyTime", "TIME", NoviSliceProcessor::delayChoices);
    addKnob ("attack", "ATTACK");
    addKnob ("release", "RELEASE");
    addKnob ("gain", "LEVEL");

    for (auto& pad : pads)
    {
        pad = std::make_unique<PadButton> (proc, fonts);
        addAndMakeVisible (*pad);
    }
    for (int i = 0; i < novi::numPerfFx; ++i)
    {
        perfButtons[(size_t) i] = std::make_unique<PerfButton> (proc, fonts, i);
        addAndMakeVisible (*perfButtons[(size_t) i]);
    }

    setSize (designW, designH);
    lcd.refresh();
}

FacePanel::~FacePanel()
{
    for (auto& [id, k] : knobs) k.attachment.reset();
    for (auto& [id, c] : choices) c.attachment.reset();
    for (auto& [id, t] : toggles) t.attachment.reset();
    setLookAndFeel (nullptr);
}

FacePanel::Knob& FacePanel::addKnob (const juce::String& id, const juce::String& label)
{
    auto& k = knobs[id];
    k.label = label;
    k.slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
    k.slider->setTextBoxStyle (juce::Slider::TextBoxBelow, true, 70, 16);
    k.slider->setRotaryParameters (juce::degreesToRadians (225.f), juce::degreesToRadians (495.f), true);
    k.slider->setMouseDragSensitivity (220);
    k.slider->setVelocityBasedMode (false);
    k.slider->setColour (juce::Slider::textBoxTextColourId, col::ink);
    k.slider->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k.slider->setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (*k.slider);
    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, *k.slider);
    if (auto* param = proc.apvts.getParameter (id))
        k.slider->setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    return k;
}

FacePanel::Choice& FacePanel::addChoice (const juce::String& id, const juce::String& label, const juce::StringArray& items)
{
    auto& c = choices[id];
    c.label = label;
    c.box = std::make_unique<juce::ComboBox>();
    c.box->addItemList (items, 1);
    addAndMakeVisible (*c.box);
    c.attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, *c.box);
    return c;
}

FacePanel::Toggle& FacePanel::addToggle (const juce::String& id, const juce::String& text)
{
    auto& t = toggles[id];
    t.button = std::make_unique<juce::TextButton> (text);
    t.button->setClickingTogglesState (true);
    addAndMakeVisible (*t.button);
    t.attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, id, *t.button);
    return t;
}

void FacePanel::placeKnob (const juce::String& id, int x, int y)
{
    // label is painted at (x, y); the knob sits underneath
    knobs[id].slider->setBounds (x, y + 16, 62, 80);
}

void FacePanel::placeChoice (const juce::String& id, int x, int y, int w)
{
    choices[id].box->setBounds (x, y + 16, w, 26);
}

void FacePanel::resized()
{
    sections.clear();
    lcd.setBounds (24, 74, 704, 240);

    // control strip
    int x = 24;
    const int y = 344, h = 34;
    auto next = [&] (juce::Component& c, int w) { c.setBounds (x, y, w, h); x += w + 9; };
    next (loadButton, 62);
    choices["bars"].box->setBounds (x, y + 4, 52, 26); x += 52 + 9;
    choices["slices"].box->setBounds (x, y + 4, 52, 26); x += 52 + 13;
    next (*toggles["triplet"].button, 66);
    next (*toggles["full"].button, 54);
    next (*toggles["loop"].button, 54);
    next (*toggles["sync"].button, 54);
    next (*toggles["keylock"].button, 72);
    x += 4;
    next (shuffleButton, 70);
    resetButton.setBounds (x, y, 728 - x, h);

    // row 1
    const int r1 = 398, r2 = 568, rowH = 156;
    sections.push_back ({ "TEXTURE", { 24, r1, 204, rowH } });
    placeKnob ("texAmount", 32, r1 + 28);
    placeKnob ("texSize", 96, r1 + 28);
    placeKnob ("vinyl", 160, r1 + 28);

    sections.push_back ({ "PLAY", { 236, r1, 204, rowH } });
    placeKnob ("pitch", 244, r1 + 28);
    placeKnob ("glide", 308, r1 + 28);
    placeKnob ("xfade", 372, r1 + 28);

    sections.push_back ({ "FILTER", { 448, r1, 280, rowH } });
    placeKnob ("cutoff", 456, r1 + 28);
    placeKnob ("res", 520, r1 + 28);
    placeKnob ("lfoDepth", 584, r1 + 28);
    placeChoice ("mode", 652, r1 + 28, 66);
    placeChoice ("lfoRate", 652, r1 + 84, 66);

    // row 2
    sections.push_back ({ "CRUSH", { 24, r2, 140, rowH } });
    placeKnob ("bits", 32, r2 + 28);
    placeKnob ("downsample", 96, r2 + 28);

    sections.push_back ({ "DELAY", { 172, r2, 216, rowH } });
    placeKnob ("dlyMix", 180, r2 + 28);
    placeKnob ("dlyFb", 244, r2 + 28);
    placeChoice ("dlyTime", 312, r2 + 28, 66);

    sections.push_back ({ "AMP ENV", { 396, r2, 140, rowH } });
    placeKnob ("attack", 404, r2 + 28);
    placeKnob ("release", 468, r2 + 28);

    sections.push_back ({ "OUTPUT", { 544, r2, 184, rowH } });
    placeKnob ("gain", 552, r2 + 28);

    // pads
    sections.push_back ({ "PADS", { 744, 74, 336, 372 } });
    bankA.setBounds (982, 78, 44, 30);
    bankB.setBounds (1030, 78, 44, 30);
    const int padSize = 74, gap = 6, gx = 752, gy = 120;
    for (int i = 0; i < 16; ++i)
    {
        const int row = 3 - i / 4, c = i % 4;   // pad 1 bottom-left, like the classic samplers
        pads[(size_t) i]->setBounds (gx + c * (padSize + gap), gy + row * (padSize + gap), padSize, padSize);
    }

    sections.push_back ({ "PERFORM", { 744, 458, 336, 266 } });
    const int py = 492, ph = 52;
    for (int i = 0; i < novi::numPerfFx; ++i)
        perfButtons[(size_t) i]->setBounds (gx + (i % 4) * (padSize + gap), py + (i / 4) * (ph + 5), padSize, ph);

    chassisCache = {};
}

void FacePanel::paintChassis (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (col::chassisTop, 0.f, 0.f, col::chassisBot, 0.f, b.getHeight(), false));
    g.fillAll();

    // fine speckle
    juce::Random rnd (11);
    for (int i = 0; i < 9000; ++i)
    {
        g.setColour ((rnd.nextBool() ? juce::Colours::black : juce::Colours::white).withAlpha (0.035f));
        g.fillRect (rnd.nextFloat() * b.getWidth(), rnd.nextFloat() * b.getHeight(), 1.f, 1.f);
    }

    // header
    g.setColour (col::ink);
    g.setFont (fonts.bold (38.f, 0.02f));
    g.drawText ("NOVISLICE", 24, 12, 220, 44, juce::Justification::centredLeft, false);
    const float logoW = juce::GlyphArrangement::getStringWidth (fonts.bold (38.f, 0.02f), "NOVISLICE");
    auto badge = Rectangle<float> (24.f + logoW + 12.f, 22.f, 58.f, 24.f);
    g.setColour (col::accent);
    g.fillRoundedRectangle (badge, 3.f);
    g.setColour (col::cream);
    g.setFont (fonts.mono (15.f, 0.04f));
    g.drawText ("NS-32", badge, juce::Justification::centred, false);
    g.setColour (col::print);
    g.setFont (fonts.semi (14.f, 0.18f));
    g.drawText ("PHRASE SAMPLER  /  PERFORMANCE SLICER", Rectangle<float> (badge.getRight() + 14.f, 20.f, 360.f, 28.f), juce::Justification::centredLeft, false);

    // vent slots
    for (int i = 0; i < 18; ++i)
    {
        auto slot = Rectangle<float> (836.f + i * 13.f, 22.f, 6.f, 26.f);
        g.setColour (Colour (0xff1e1c1a));
        g.fillRoundedRectangle (slot, 3.f);
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.drawLine (slot.getX() + 1.f, slot.getBottom() + 1.f, slot.getRight() - 1.f, slot.getBottom() + 1.f, 1.f);
    }

    // pinstripe
    g.setColour (col::accent);
    g.fillRect (0.f, 60.f, b.getWidth(), 3.f);
    g.setColour (col::ink);
    g.fillRect (0.f, 64.f, b.getWidth(), 1.5f);

    // screws
    for (auto p : { juce::Point<float> (10.f, 10.f), { b.getWidth() - 10.f, 10.f }, { 10.f, b.getHeight() - 10.f }, { b.getWidth() - 10.f, b.getHeight() - 10.f } })
    {
        auto s = Rectangle<float> (10.f, 10.f).withCentre (p);
        g.setGradientFill (juce::ColourGradient (Colour (0xffb8b2a5), s.getX(), s.getY(), Colour (0xff6f6a61), s.getRight(), s.getBottom(), false));
        g.fillEllipse (s);
        g.setColour (Colour (0xff3e3a35));
        g.drawLine ({ s.getCentre().translated (-3.f, 2.f), s.getCentre().translated (3.f, -2.f) }, 1.4f);
    }

    // control strip labels
    g.setFont (fonts.mono (10.5f));
    g.setColour (col::printLight);
    g.drawText ("BARS", choices["bars"].box->getBounds().translated (0, 28).withHeight (12), juce::Justification::centred, false);
    g.drawText ("PER BAR", choices["slices"].box->getBounds().translated (0, 28).withHeight (12), juce::Justification::centred, false);
    g.drawText ("SLICE ORDER", shuffleButton.getBounds().getUnion (resetButton.getBounds()).translated (0, 38).withHeight (12), juce::Justification::centred, false);
    g.drawText ("PLAYBACK", toggles["triplet"].button->getBounds().getUnion (toggles["keylock"].button->getBounds()).translated (0, 38).withHeight (12),
                juce::Justification::centred, false);

    // sections
    for (auto& s : sections)
    {
        auto r = s.area.toFloat();
        g.setColour (col::print.withAlpha (0.55f));
        g.drawRoundedRectangle (r, 6.f, 1.4f);
        g.setFont (fonts.semi (13.f, 0.16f));
        const float tw = juce::GlyphArrangement::getStringWidth (fonts.semi (13.f, 0.16f), s.title) + 18.f;
        auto tab = Rectangle<float> (r.getX() + 10.f, r.getY() - 9.f, tw, 18.f);
        g.setColour (col::chassisTop.interpolatedWith (col::chassisBot, r.getY() / b.getHeight()));
        g.fillRect (tab.expanded (3.f, 0.f));
        g.setColour (col::ink);
        g.fillRoundedRectangle (tab, 3.f);
        g.setColour (col::cream);
        g.drawText (s.title, tab, juce::Justification::centred, false);
    }

    // knob and choice labels
    g.setFont (fonts.semi (12.5f, 0.1f));
    g.setColour (col::ink);
    for (auto& [id, k] : knobs)
        g.drawText (k.label, k.slider->getBounds().translated (0, -16).withHeight (14), juce::Justification::centred, false);
    for (auto& [id, c] : choices)
        if (id != "bars" && id != "slices")
            g.drawText (c.label, c.box->getBounds().translated (0, -16).withHeight (14), juce::Justification::centred, false);

    // hints
    g.setFont (fonts.mono (10.5f));
    g.setColour (col::printLight);
    g.drawText ("SLICES ON WHITE KEYS FROM C5", Rectangle<int> (754, 92, 220, 14), juce::Justification::centredLeft, false);
    g.drawText ("HOLD TO PLAY  -  SHIFT-CLICK TO LATCH  -  KEYS C3-D#4", Rectangle<int> (752, 470, 320, 14), juce::Justification::centredLeft, false);
    g.drawText ("CLICK THE SCREEN TO AUDITION  -  DRAG A FILE ONTO IT TO LOAD", Rectangle<int> (24, 318, 704, 14), juce::Justification::centredLeft, false);

    // output meter frame
    auto meter = Rectangle<float> (636.f, (float) meterY, 18.f, 116.f);
    g.setColour (col::bezel);
    g.fillRoundedRectangle (meter.expanded (4.f), 4.f);
    g.setColour (col::print);
    g.setFont (fonts.mono (10.f));
    g.drawText ("+0", Rectangle<float> (meter.getRight() + 8.f, meter.getY() - 2.f, 30.f, 12.f), juce::Justification::centredLeft, false);
    g.drawText ("-12", Rectangle<float> (meter.getRight() + 8.f, meter.getCentreY() - 6.f, 30.f, 12.f), juce::Justification::centredLeft, false);
    g.drawText ("-48", Rectangle<float> (meter.getRight() + 8.f, meter.getBottom() - 10.f, 30.f, 12.f), juce::Justification::centredLeft, false);
}

void FacePanel::paint (juce::Graphics& g)
{
    if (chassisCache.isNull())
    {
        chassisCache = juce::Image (juce::Image::RGB, designW * 2, designH * 2, true);
        juce::Graphics cg (chassisCache);
        cg.addTransform (juce::AffineTransform::scale (2.f));
        paintChassis (cg);
    }
    g.drawImage (chassisCache, getLocalBounds().toFloat());
}

void FacePanel::paintOverChildren (juce::Graphics& g)
{
    // output meter LEDs
    auto meter = Rectangle<float> (636.f, (float) meterY, 18.f, 116.f);
    const int segs = 14;
    const float segH = meter.getHeight() / segs;
    const float db = juce::Decibels::gainToDecibels (meterLevel, -60.f);
    for (int i = 0; i < segs; ++i)
    {
        const float segDb = -48.f + 48.f * (i + 1) / segs;
        auto seg = Rectangle<float> (meter.getX(), meter.getBottom() - (i + 1) * segH, meter.getWidth(), segH).reduced (1.5f, 1.2f);
        const bool lit = db >= segDb - 48.f / segs;
        const Colour c = i >= segs - 2 ? col::ledOn : i >= segs - 5 ? Colour (0xffffb020) : Colour (0xff9fd04a);
        g.setColour (lit ? c : c.withMultipliedBrightness (0.25f).withAlpha (0.8f));
        g.fillRect (seg);
    }
}

void FacePanel::refresh()
{
    lcd.refresh();
    const int total = proc.getNumSlices();
    bankB.setEnabled (total > 16);
    if (total <= 16 && bank == 1)
    {
        bank = 0;
        bankA.setToggleState (true, juce::dontSendNotification);
    }
    const int activeKey = proc.engine.uiActiveKey.load();
    const bool loaded = proc.hasSample();
    for (int i = 0; i < 16; ++i)
    {
        const int key = bank * 16 + i;
        pads[(size_t) i]->setKey (key, loaded && key < total);
        pads[(size_t) i]->setLit (key == activeKey);
        pads[(size_t) i]->repaint();
    }
    const auto mask = proc.engine.uiPerfMask.load();
    for (int i = 0; i < novi::numPerfFx; ++i)
        perfButtons[(size_t) i]->setLit ((mask >> i) & 1u);

    const float peak = proc.outputPeak.exchange (0.f);
    meterLevel = juce::jmax (peak, meterLevel * 0.85f);
    repaint (Rectangle<int> (630, meterY - 6, 30, 130));
}

void FacePanel::loadViaChooser()
{
    chooser = std::make_unique<juce::FileChooser> ("Load a loop", juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                   "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (f.existsAsFile())
                                  proc.loadFile (f);
                              lcd.refresh();
                          });
}

// =====================================================================

NoviSliceEditor::NoviSliceEditor (NoviSliceProcessor& p) : AudioProcessorEditor (&p), proc (p), panel (p)
{
    addAndMakeVisible (panel);
    setResizable (true, true);
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio ((double) FacePanel::designW / FacePanel::designH);
        c->setSizeLimits (770, 515, 1650, 1104);
    }
    setSize (990, 662);
    startTimerHz (30);
}

NoviSliceEditor::~NoviSliceEditor() { stopTimer(); }

void NoviSliceEditor::resized()
{
    const float scale = (float) getWidth() / FacePanel::designW;
    panel.setTransform (juce::AffineTransform::scale (scale));
}

void NoviSliceEditor::timerCallback() { panel.refresh(); }

static bool isAudioFile (const juce::String& path)
{
    return juce::File (path).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg");
}

bool NoviSliceEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (isAudioFile (f)) return true;
    return false;
}

void NoviSliceEditor::fileDragEnter (const juce::StringArray&, int, int) { panel.getLcd().dropHover = true; panel.getLcd().repaint(); }
void NoviSliceEditor::fileDragExit (const juce::StringArray&)           { panel.getLcd().dropHover = false; panel.getLcd().repaint(); }

void NoviSliceEditor::filesDropped (const juce::StringArray& files, int, int)
{
    panel.getLcd().dropHover = false;
    for (auto& f : files)
        if (isAudioFile (f) && proc.loadFile (juce::File (f)))
            break;
    panel.getLcd().refresh();
}
