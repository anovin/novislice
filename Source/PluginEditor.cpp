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
    const int sv = proc.sliceVersion.load();
    if (sv != lastSliceVersion)
    {
        lastSliceVersion = sv;
        map = proc.getSliceMap();
    }
    repaint();
}

void LcdDisplay::flash (const juce::String& m)
{
    message = m;
    messageUntil = juce::Time::getMillisecondCounter() + 2800;
    repaint();
}

juce::Rectangle<float> LcdDisplay::waveArea() const
{
    auto inner = getLocalBounds().toFloat().reduced (12.f).reduced (10.f, 8.f);
    inner.removeFromTop (20.f);
    inner.removeFromBottom (18.f);
    return inner.reduced (0.f, 4.f);
}

double LcdDisplay::toFraction (float x) const
{
    const auto w = waveArea();
    return juce::jlimit (0.0, 1.0, (double) (x - w.getX()) / w.getWidth());
}

float LcdDisplay::toX (double fraction) const
{
    const auto w = waveArea();
    return w.getX() + (float) fraction * w.getWidth();
}

LcdDisplay::Grab LcdDisplay::hitTest (juce::Point<float> p, int& slice) const
{
    slice = -1;
    if (! editing || map.count == 0)
        return Grab::none;
    const auto w = waveArea();
    auto f = [this] (const char* id) { return (double) proc.apvts.getRawParameterValue (id)->load(); };

    // trim flags sit along the top edge of the wave
    if (p.y < w.getY() + 14.f)
    {
        if (std::abs (p.x - toX (f ("trimStart"))) < 8.f) return Grab::trimStart;
        if (std::abs (p.x - toX (f ("trimEnd"))) < 8.f) return Grab::trimEnd;
    }
    float best = 6.f;
    Grab g = Grab::none;
    for (int i = 0; i < map.count; ++i)
    {
        const float xs = toX (map.s[(size_t) i].start), xe = toX (map.s[(size_t) i].end);
        if (i > 0 && std::abs (p.x - xs) < best) { best = std::abs (p.x - xs); g = Grab::sliceStart; slice = i; }
        const bool contiguous = i + 1 < map.count && std::abs (map.s[(size_t) i + 1].start - map.s[(size_t) i].end) < 1e-6;
        if (! contiguous && std::abs (p.x - xe) < best) { best = std::abs (p.x - xe); g = Grab::sliceEnd; slice = i; }
    }
    return g;
}

LcdDisplay::ZoomInfo LcdDisplay::zoomInfo() const
{
    ZoomInfo z;
    if (zoomPad < 0 || map.count == 0 || ! proc.hasSample())
        return z;
    z.slice = proc.getSliceForKey (zoomPad);
    if (z.slice < 0 || z.slice >= map.count)
        return z;
    const double seconds = proc.getSampleSeconds();
    if (seconds <= 0.0) return z;
    z.k = 0.001 / seconds;                       // fraction of the sample per millisecond
    z.pad = proc.getPad (zoomPad);
    z.s0 = map.s[(size_t) z.slice].start;
    z.s1 = map.s[(size_t) z.slice].end;
    z.start = juce::jlimit (0.0, 1.0, z.s0 + z.pad.startMs * z.k);
    z.end = juce::jlimit (z.start, 1.0, z.s1 + z.pad.endMs * z.k);
    z.tailEnd = juce::jmin (1.0, z.end + z.pad.tailMs * z.k);
    z.valid = true;
    return z;
}

float LcdDisplay::zoomX (double f) const
{
    const auto w = waveArea();
    return w.getX() + (float) ((f - viewA) / juce::jmax (1e-9, viewB - viewA)) * w.getWidth();
}

double LcdDisplay::zoomFraction (float x) const
{
    const auto w = waveArea();
    return juce::jlimit (0.0, 1.0, viewA + (double) (x - w.getX()) / w.getWidth() * (viewB - viewA));
}

void LcdDisplay::paintZoom (juce::Graphics& g, juce::Rectangle<float> wave, juce::Rectangle<float> numbers, const ZoomInfo& z)
{
    if (! zoomDragging)
    {
        const double lo = juce::jmin (z.start, z.s0), hi = juce::jmax (z.tailEnd, z.s1);
        const double margin = juce::jmax ((hi - lo) * 0.2, 40.0 * z.k);
        viewA = juce::jmax (0.0, lo - margin);
        viewB = juce::jmin (1.0, hi + margin);
    }
    const int cols = juce::jmax (8, (int) (wave.getWidth() / 2.f));
    const int version = proc.sampleVersion.load();
    if (std::abs (viewA - peaksA) > 1e-12 || std::abs (viewB - peaksB) > 1e-12 || version != peaksVersion || (int) zoomPeaks.size() != cols * 2)
    {
        zoomPeaks = proc.getPeaksRange (viewA, viewB, cols);
        peaksA = viewA; peaksB = viewB; peaksVersion = version;
    }

    float maxAbs = 0.01f;
    for (float v : zoomPeaks) maxAbs = juce::jmax (maxAbs, std::abs (v));
    const float mid = wave.getCentreY(), half = wave.getHeight() * 0.5f;
    for (int c = 0; c < cols; ++c)
    {
        const double f = viewA + (c + 0.5) / cols * (viewB - viewA);
        if (f >= z.start && f < z.end)          g.setColour (col::lcdInk);
        else if (f >= z.end && f < z.tailEnd)   g.setColour (col::lcdInk.withAlpha (0.55f));
        else                                    g.setColour (col::lcdDim.withAlpha (0.5f));
        const float lo = zoomPeaks[(size_t) c * 2], hi = zoomPeaks[(size_t) c * 2 + 1];
        const float top = mid - (hi / maxAbs) * half, bottom = mid - (lo / maxAbs) * half;
        g.fillRect (wave.getX() + c * 2.f, std::floor (top), 1.5f, juce::jmax (1.f, std::ceil (bottom - top)));
    }

    // the slice's own markers (dotted), and the next slice's start
    g.setColour (col::lcdInk.withAlpha (0.6f));
    std::vector<double> marks { z.s0, z.s1 };
    for (double m : marks)
    {
        const float x = zoomX (m);
        if (x < wave.getX() || x > wave.getRight()) continue;
        for (float yy = wave.getY() - 4.f; yy < wave.getBottom() + 4.f; yy += 4.f)
            g.fillRect (std::round (x), yy, 1.f, 2.f);
    }

    // fades
    const double fadeIn = z.pad.fadeInMs * z.k;
    const double fadeOut = (z.pad.fadeOutMs > 0.f ? z.pad.fadeOutMs : proc.apvts.getRawParameterValue ("xfade")->load()) * z.k;
    g.setColour (col::lcdInk);
    if (fadeIn > 0.0)
        g.drawLine (zoomX (z.start), wave.getBottom(), zoomX (z.start + fadeIn), wave.getY(), 1.5f);
    const double endPoint = z.tailEnd > z.end ? z.tailEnd : z.end;
    g.drawLine (zoomX (endPoint - fadeOut), wave.getY(), zoomX (endPoint), wave.getBottom(), 1.5f);

    // S / E handles
    for (int which = 0; which < 2; ++which)
    {
        const float x = zoomX (which == 0 ? z.start : z.end);
        g.setColour (col::lcdInk);
        g.fillRect (x - 1.f, wave.getY() - 4.f, 2.f, wave.getHeight() + 8.f);
        auto tab = Rectangle<float> (which == 0 ? x : x - 14.f, wave.getY() - 4.f, 14.f, 14.f);
        g.fillRect (tab);
        g.setColour (col::lcdTop);
        g.setFont (fonts.mono (12.f));
        g.drawText (which == 0 ? "S" : "E", tab, juce::Justification::centred, false);
    }

    // playhead
    const float ph = proc.engine.uiPlayhead.load();
    if (ph >= 0.f && ph >= viewA && ph <= viewB)
    {
        g.setColour (col::lcdInk);
        g.fillRect (zoomX (ph) - 1.f, wave.getY() - 4.f, 2.f, wave.getHeight() + 8.f);
    }

    // times under the handles
    const double seconds = proc.getSampleSeconds();
    g.setColour (col::lcdInk);
    g.setFont (fonts.mono (12.f));
    g.drawText (juce::String (z.start * seconds, 3) + " S", Rectangle<float> (zoomX (z.start), numbers.getY(), 90.f, numbers.getHeight()),
                juce::Justification::centredLeft, false);
    g.drawText (juce::String (z.end * seconds, 3) + " S", Rectangle<float> (zoomX (z.end) - 90.f, numbers.getY(), 90.f, numbers.getHeight()),
                juce::Justification::centredRight, false);
}

void LcdDisplay::mouseMove (const juce::MouseEvent& e)
{
    if (const auto z = zoomInfo(); z.valid)
    {
        const bool near = std::abs (e.position.x - zoomX (z.start)) < 8.f || std::abs (e.position.x - zoomX (z.end)) < 8.f;
        setMouseCursor (near ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
        return;
    }
    int s;
    const auto g = hitTest (e.position, s);
    setMouseCursor (g == Grab::none ? juce::MouseCursor::NormalCursor : juce::MouseCursor::LeftRightResizeCursor);
}

void LcdDisplay::mouseDown (const juce::MouseEvent& e)
{
    if (! proc.hasSample())
        return;
    if (const auto z = zoomInfo(); z.valid)
    {
        const float ds = std::abs (e.position.x - zoomX (z.start)), de = std::abs (e.position.x - zoomX (z.end));
        if (juce::jmin (ds, de) < 8.f)
        {
            grab = ds <= de ? Grab::padStart : Grab::padEnd;
            zoomDragging = true;
            return;
        }
        auditionNote = novi::noteForSliceKey (zoomPad);        // click: hear the pad as it is now
        if (auditionNote >= 0)
            proc.keyboardState.noteOn (1, auditionNote, 0.9f);
        return;
    }
    int s;
    grab = hitTest (e.position, grabSlice);
    if (grab != Grab::none)
    {
        if (e.mods.isAltDown() && grab == Grab::sliceStart)
        {
            proc.removeSliceMarker (grabSlice);   // Alt-click a marker to delete it
            grab = Grab::none;
            refresh();
        }
        return;
    }
    juce::ignoreUnused (s);

    // Click a slice: select it and hear it.
    const double fr = toFraction (e.position.x);
    int slice = -1;
    for (int i = 0; i < map.count; ++i)
        if (fr >= map.s[(size_t) i].start && fr < map.s[(size_t) i].end) { slice = i; break; }
    if (slice < 0) return;
    selectedSlice = slice;
    if (onSliceClicked) onSliceClicked (slice);
    auditionNote = novi::noteForSliceKey (proc.getKeyForSlice (slice));
    if (auditionNote >= 0)
        proc.keyboardState.noteOn (1, auditionNote, 0.9f);
}

void LcdDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (grab == Grab::none) return;
    if (grab == Grab::padStart || grab == Grab::padEnd)
    {
        const auto z = zoomInfo();
        if (! z.valid) return;
        const double f = proc.snapFraction (zoomFraction (e.position.x));
        auto pd = z.pad;
        const double minLen = 5.0 * z.k;
        if (grab == Grab::padStart)
            pd.startMs = (float) juce::jlimit (-250.0, 250.0, (juce::jmin (f, z.end - minLen) - z.s0) / z.k);
        else
            pd.endMs = (float) juce::jlimit (-250.0, 250.0, (juce::jmax (f, z.start + minLen) - z.s1) / z.k);
        proc.setPad (zoomPad, pd);
        if (onPadEdited) onPadEdited();
        repaint();
        return;
    }
    const double fr = toFraction (e.position.x);
    auto f = [this] (const char* id) { return (double) proc.apvts.getRawParameterValue (id)->load(); };
    switch (grab)
    {
        case Grab::sliceStart: proc.moveSliceEdge (grabSlice, true, fr); break;
        case Grab::sliceEnd:   proc.moveSliceEdge (grabSlice, false, fr); break;
        case Grab::trimStart:  proc.setTrim (std::min (fr, f ("trimEnd") - 0.005), f ("trimEnd")); break;
        case Grab::trimEnd:    proc.setTrim (f ("trimStart"), std::max (fr, f ("trimStart") + 0.005)); break;
        default: break;
    }
    refresh();
}

void LcdDisplay::mouseUp (const juce::MouseEvent&)
{
    grab = Grab::none;
    zoomDragging = false;
    if (auditionNote >= 0)
        proc.keyboardState.noteOff (1, auditionNote, 0.f);
    auditionNote = -1;
}

void LcdDisplay::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (zoomInfo().valid) return;
    if (! editing || ! proc.hasSample() || ! waveArea().expanded (0.f, 6.f).contains (e.position))
        return;
    if (auditionNote >= 0) { proc.keyboardState.noteOff (1, auditionNote, 0.f); auditionNote = -1; }
    const int added = proc.addSliceMarker (toFraction (e.position.x));
    if (added >= 0) { selectedSlice = added; if (onSliceClicked) onSliceClicked (added); }
    refresh();
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
    const auto wave = waveArea();

    // status line
    const int total = map.count;
    const double bpm = proc.getLoopBpm();
    g.setColour (col::lcdInk);
    g.setFont (fonts.mono (15.f));
    juce::String left = bpm > 0.0 ? "BPM " + juce::String (bpm, 1) : juce::String ("BPM ---.-");
    if (bpm > 0.0 && ! proc.isLoopBpmManual()) left << "?";
    const double host = proc.hostBpm.load();
    if (bpm > 0.0 && host > 0.0 && proc.apvts.getRawParameterValue ("sync")->load() > 0.5f)
        left << " > " << juce::String (host, 1);
    static const char* modeTags[] = { "GRID", "REG", "TRANS", "MAN" };
    left << "  " << juce::String (total).paddedLeft ('0', 2) << " " << modeTags[juce::jlimit (0, 3, proc.getChopMode())];
    const bool showMessage = message.isNotEmpty() && juce::Time::getMillisecondCounter() < messageUntil;
    if (proc.engine.uiPreviewing.load() && ! showMessage)
    {
        const float ph = proc.engine.uiPlayhead.load();
        left << "  PLAYING " << juce::String (juce::jmax (0.f, ph) * proc.getSampleSeconds(), 2) << " / "
             << juce::String (proc.getSampleSeconds(), 2) << " S";
    }
    else if (showMessage)
        left << "  " << message;
    else if (name.isNotEmpty())
        left << "  " << name.toUpperCase();
    const auto zoom = zoomInfo();
    if (zoom.valid && ! showMessage)
    {
        auto ms = [] (float v) { return std::abs (v) < 0.05f ? juce::String ("0") : (v > 0 ? "+" : "") + juce::String (v, 1); };
        left = "PAD " + juce::String (zoomPad + 1).paddedLeft ('0', 2) + "  SLICE " + juce::String (zoom.slice + 1).paddedLeft ('0', 2)
             + "  START " + ms (zoom.pad.startMs) + "  END " + ms (zoom.pad.endMs) + " MS  LEN "
             + juce::String ((zoom.end - zoom.start) * proc.getSampleSeconds(), 3) + " S";
    }
    auto flagsArea = status.removeFromRight (208.f);
    g.drawText (left, status, juce::Justification::centredLeft, true);

    auto flag = [&] (const juce::String& text, bool on)
    {
        auto f = flagsArea.removeFromRight (juce::GlyphArrangement::getStringWidth (fonts.mono (13.f), text) + 10.f);
        flagsArea.removeFromRight (3.f);
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
    flag ("SYNC", on ("sync"));
    flag ("RPT", on ("noteRepeat"));
    flag ("16L", on ("sixteen"));

    const bool loaded = ! peaks.empty() && proc.hasSample();
    if (loaded && zoom.valid)
        paintZoom (g, wave, numbers, zoom);
    else if (! loaded)
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
        auto pf = [this] (const char* id) { return (double) proc.apvts.getRawParameterValue (id)->load(); };
        const double trimS = pf ("trimStart"), trimE = pf ("trimEnd");

        auto sliceAt = [&] (double fr)
        {
            for (int i = 0; i < total; ++i)
                if (fr >= map.s[(size_t) i].start && fr < map.s[(size_t) i].end) return i;
            return -1;
        };

        if (activeSlice >= 0 && activeSlice < total)
        {
            const float xa = toX (map.s[(size_t) activeSlice].start), xb = toX (map.s[(size_t) activeSlice].end);
            g.setColour (col::lcdInk);
            g.fillRect (Rectangle<float> (xa, wave.getY() - 4.f, juce::jmax (1.f, xb - xa), wave.getHeight() + 8.f));
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
            const double fr = (c + 0.5) / cols;
            const int slice = sliceAt (fr);
            if (slice < 0 || fr < trimS || fr > trimE)
                g.setColour (col::lcdDim.withAlpha (0.55f));
            else
                g.setColour (slice == activeSlice ? col::lcdTop : col::lcdInk);
            const float top = mid - (hi / maxAbs) * half, bottom = mid - (lo / maxAbs) * half;
            g.fillRect (x, std::floor (top), 2.f, juce::jmax (1.f, std::ceil (bottom - top)));
        }

        // slice markers + numbers
        for (int s = 0; s < total; ++s)
        {
            const float xs = toX (map.s[(size_t) s].start), xe = toX (map.s[(size_t) s].end);
            const bool inverted = s == activeSlice || (s > 0 && s - 1 == activeSlice);
            g.setColour (inverted ? col::lcdTop : col::lcdInk.withAlpha (editing ? 0.85f : 0.55f));
            for (float yy = wave.getY() - 4.f; yy < wave.getBottom() + 4.f; yy += 4.f)
                g.fillRect (std::round (xs), yy, editing ? 1.5f : 1.f, 2.f);
            if (editing && s > 0)
                g.fillRect (std::round (xs) - 3.f, wave.getBottom() + 1.f, 7.f, 3.f);   // grab handle

            const bool contiguous = s + 1 < total && std::abs (map.s[(size_t) s + 1].start - map.s[(size_t) s].end) < 1e-6;
            if (! contiguous)
            {
                g.setColour (col::lcdInk);
                g.fillRect (std::round (xe) - 1.f, wave.getY() - 4.f, 2.f, wave.getHeight() + 8.f);
            }

            if (s == selectedSlice && s != activeSlice)
            {
                g.setColour (col::lcdInk);
                g.fillRect (xs, wave.getY() - 6.f, xe - xs, 2.f);
                g.fillRect (xs, wave.getBottom() + 4.f, xe - xs, 2.f);
            }

            const float w = xe - xs;
            if (w >= 15.f || (w >= 8.f && s % 2 == 0))
            {
                g.setColour (s == activeSlice || s == selectedSlice ? col::lcdInk : col::lcdDim);
                g.setFont (fonts.mono (w >= 15.f ? 12.f : 10.f));
                g.drawText (juce::String (s + 1).paddedLeft ('0', 2), Rectangle<float> (xs, numbers.getY(), juce::jmax (w, 16.f), numbers.getHeight()),
                            juce::Justification::centred, false);
            }
        }

        if (total > 0 && map.regionEnd < trimE - 1e-4)
        {
            const float xr = toX (map.regionEnd);
            g.setColour (col::lcdDim);
            g.setFont (fonts.mono (12.f));
            g.drawText ("NOT SLICED", Rectangle<float> (xr + 4.f, numbers.getY(), wave.getRight() - xr - 4.f, numbers.getHeight()),
                        juce::Justification::centredLeft, false);
        }

        // trim flags
        if (editing || trimS > 0.0 || trimE < 1.0)
            for (int which = 0; which < 2; ++which)
            {
                const float x = toX (which == 0 ? trimS : trimE);
                g.setColour (col::lcdInk);
                g.fillRect (x - 1.f, wave.getY() - 4.f, 2.f, wave.getHeight() + 8.f);
                auto tab = Rectangle<float> (which == 0 ? x : x - 12.f, wave.getY() - 4.f, 12.f, 12.f);
                g.fillRect (tab);
                g.setColour (col::lcdTop);
                g.setFont (fonts.mono (11.f));
                g.drawText (which == 0 ? "S" : "E", tab, juce::Justification::centred, false);
            }

        const float ph = proc.engine.uiPlayhead.load();
        if (ph >= 0.f)
        {
            const float x = toX (ph);
            g.setColour (activeSlice >= 0 && sliceAt (ph) == activeSlice ? col::lcdTop : col::lcdInk);
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

// =====================================================================

PadButton::PadButton (NoviSliceProcessor& p, const NoviFonts& f) : proc (p), fonts (f) {}

void PadButton::setState (int k, bool en, bool l, bool sel, int lvl)
{
    const auto pd = proc.getPad (k);
    const bool changed = k != key || en != enabled || l != lit || sel != selected || lvl != level
                         || pd.mute != shown.mute || pd.solo != shown.solo || pd.playMode != shown.playMode || pd.motion != shown.motion;
    key = k; enabled = en; lit = l; selected = sel; level = lvl; shown = pd;
    if (changed) repaint();
}

void PadButton::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colour (0xff2b2926));
    g.fillRoundedRectangle (r, 9.f);
    g.setColour (selected ? col::cream.withAlpha (0.9f) : juce::Colours::black.withAlpha (0.4f));
    g.drawRoundedRectangle (r.reduced (selected ? 1.f : 0.5f), 9.f, selected ? 2.f : 1.f);

    auto pad = r.reduced (5.f).withTrimmedBottom (pressed ? 0.f : 2.f).translated (0.f, pressed ? 2.f : 0.f);
    const bool muted = shown.mute || (proc.anyPadSolo() && ! shown.solo && level < 0);
    const float alpha = ! enabled ? 0.35f : muted ? 0.55f : 1.f;
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
    g.setColour (juce::Colours::white.withAlpha (enabled ? 0.18f : 0.06f));
    g.drawLine (pad.getX() + 6.f, pad.getY() + 1.5f, pad.getRight() - 6.f, pad.getY() + 1.5f, 1.f);
    g.setColour (juce::Colours::black.withAlpha (0.15f));
    g.drawRoundedRectangle (pad, 7.f, 1.f);

    const auto ink = lit ? Colour (0xff3b160a) : col::ink.withAlpha (enabled ? 0.85f : 0.4f);
    g.setColour (ink);
    auto txt = pad.reduced (7.f, 5.f);
    g.setFont (fonts.bold (20.f));
    juce::String big = "--";
    if (level >= 0) big = "L" + juce::String (level + 1).paddedLeft ('0', 2);
    else if (enabled) big = juce::String (proc.getSliceForKey (key) + 1).paddedLeft ('0', 2);
    g.drawText (big, txt, juce::Justification::topLeft, false);
    g.setFont (fonts.mono (11.f));
    const int note = novi::noteForSliceKey (level >= 0 ? level : key);
    if (note >= 0)
        g.drawText (noteName (note), txt, juce::Justification::bottomRight, false);

    // small flags: mute, solo, play mode, motion
    juce::StringArray tags;
    if (level < 0)
    {
        if (shown.mute) tags.add ("M");
        if (shown.solo) tags.add ("S");
        if (shown.playMode == novi::playOneShot) tags.add ("1");
        if (shown.playMode == novi::playNoteOff) tags.add ("R");
        if (shown.motion != novi::motionOff) tags.add (">");
    }
    float x = txt.getRight();
    for (int i = tags.size(); --i >= 0;)
    {
        auto box = Rectangle<float> (x - 13.f, txt.getY() + 1.f, 13.f, 13.f);
        x -= 15.f;
        g.setColour (ink);
        g.fillRoundedRectangle (box, 2.f);
        g.setColour (lit ? Colour (0xffff8a52) : col::padTop);
        g.setFont (fonts.mono (10.f));
        g.drawText (tags[i], box, juce::Justification::centred, false);
    }
}

void PadButton::mouseDown (const juce::MouseEvent&)
{
    if (! enabled) return;
    pressed = true;
    draggedOut = false;
    repaint();
    if (level < 0 && onSelect) onSelect (key);
    const int note = novi::noteForSliceKey (level >= 0 ? level : key);
    if (note >= 0)
        proc.keyboardState.noteOn (1, note, 0.9f);
}

void PadButton::release()
{
    if (! pressed) return;
    pressed = false;
    repaint();
    const int note = novi::noteForSliceKey (level >= 0 ? level : key);
    if (note >= 0)
        proc.keyboardState.noteOff (1, note, 0.f);
}

void PadButton::mouseDrag (const juce::MouseEvent& e)
{
    // Drag a pad out of the plugin: its slice becomes a WAV you can drop into FL Studio.
    if (! enabled || draggedOut || level >= 0 || getLocalBounds().contains (e.getPosition()))
        return;
    draggedOut = true;
    release();
    const auto file = proc.writeSliceToTempFile (key);
    if (file.existsAsFile())
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this);
}

void PadButton::mouseUp (const juce::MouseEvent&)
{
    release();
}

// =====================================================================

void DragFileButton::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 6)
        return;
    dragging = true;
    const auto file = makeFile ? makeFile() : juce::File();
    if (file.existsAsFile())
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this);
}

void DragFileButton::mouseUp (const juce::MouseEvent& e)
{
    dragging = false;
    juce::TextButton::mouseUp (e);
}

// =====================================================================

BpmBox::BpmBox (NoviSliceProcessor& p, const NoviFonts& f)
    : proc (p), fonts (f),
      attachment (*p.apvts.getParameter ("sampleBpm"), [this] (float v) { value = v; repaint(); }, nullptr)
{
    attachment.sendInitialUpdate();
    editor.setFont (fonts.mono (15.f));
    editor.setJustification (juce::Justification::centred);
    editor.setColour (juce::TextEditor::backgroundColourId, col::lcdTop);
    editor.setColour (juce::TextEditor::textColourId, col::lcdInk);
    editor.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    editor.setColour (juce::TextEditor::focusedOutlineColourId, col::lcdInk);
    editor.setColour (juce::CaretComponent::caretColourId, col::lcdInk);
    editor.setInputRestrictions (6, "0123456789.");
    editor.onReturnKey = [this] { commitText(); };
    editor.onFocusLost = [this] { commitText(); };
    editor.onEscapeKey = [this] { editor.setVisible (false); };
    addChildComponent (editor);
    setTooltip ("Loop BPM: drag or use the arrows to set the sample's tempo, double-click to type it. Below 40 = AUTO (worked out from BARS).");
}

double BpmBox::startingTempo() const
{
    if (value >= novi::minSampleBpm) return value;
    const double guess = proc.getLoopBpm();
    return guess >= novi::minSampleBpm ? std::round (guess * 10.0) / 10.0 : 120.0;
}

void BpmBox::set (double bpm)
{
    const float v = bpm >= novi::minSampleBpm ? (float) juce::jlimit ((double) novi::minSampleBpm, (double) novi::maxSampleBpm, bpm) : 0.f;
    attachment.setValueAsCompleteGesture (v);
}

void BpmBox::commitText()
{
    if (! editor.isVisible()) return;
    const auto t = editor.getText().trim();
    editor.setVisible (false);
    set (t.isEmpty() ? 0.0 : t.getDoubleValue());
}

juce::Rectangle<float> BpmBox::arrowArea() const
{
    return getLocalBounds().toFloat().reduced (3.f).removeFromRight (15.f);
}

void BpmBox::resized()
{
    editor.setBounds (getLocalBounds().reduced (3).withTrimmedRight (15));
}

void BpmBox::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (col::bezel);
    g.fillRoundedRectangle (r, 4.f);
    auto glass = r.reduced (3.f);
    g.setGradientFill (juce::ColourGradient (col::lcdTop, 0.f, glass.getY(), col::lcdBot, 0.f, glass.getBottom(), false));
    g.fillRoundedRectangle (glass, 2.f);

    const bool manual = value >= novi::minSampleBpm;
    g.setColour (manual ? col::lcdInk : col::lcdDim);
    g.setFont (fonts.mono (15.f));
    g.drawText (manual ? juce::String (value, 1) : juce::String ("AUTO"), glass.withTrimmedRight (13.f), juce::Justification::centred, false);

    auto arrows = arrowArea();
    g.setColour (col::lcdInk.withAlpha (0.25f));
    g.fillRect (arrows.getX(), arrows.getY() + 2.f, 1.f, arrows.getHeight() - 4.f);
    g.setColour (col::lcdInk);
    const float cx = arrows.getCentreX() + 1.f, cy = arrows.getCentreY();
    juce::Path up, down;
    up.addTriangle (cx - 4.f, cy - 2.f, cx + 4.f, cy - 2.f, cx, cy - 7.f);
    down.addTriangle (cx - 4.f, cy + 2.f, cx + 4.f, cy + 2.f, cx, cy + 7.f);
    g.fillPath (up);
    g.fillPath (down);
}

void BpmBox::mouseDown (const juce::MouseEvent& e)
{
    dragging = false;
    if (arrowArea().contains (e.position))
    {
        const double step = e.mods.isShiftDown() ? 0.1 : 1.0;
        const bool upArrow = e.position.y < arrowArea().getCentreY();
        if (value < novi::minSampleBpm)
            set (upArrow ? startingTempo() : 0.0);       // from AUTO, the first press locks in the detected tempo
        else
        {
            const double next = value + (upArrow ? step : -step);
            set (next < novi::minSampleBpm ? 0.0 : next);
        }
        return;
    }
    dragStart = startingTempo();
    dragging = true;
    attachment.beginGesture();
}

void BpmBox::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging) return;
    const double perPixel = e.mods.isShiftDown() ? 0.02 : 0.2;
    double v = dragStart - e.getDistanceFromDragStartY() * perPixel;
    v = std::round (v * (e.mods.isShiftDown() ? 100.0 : 10.0)) / (e.mods.isShiftDown() ? 100.0 : 10.0);
    attachment.setValueAsPartOfGesture ((float) juce::jlimit ((double) novi::minSampleBpm, (double) novi::maxSampleBpm, v));
}

void BpmBox::mouseUp (const juce::MouseEvent&)
{
    if (dragging) attachment.endGesture();
    dragging = false;
}

void BpmBox::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (arrowArea().contains (e.position)) return;
    editor.setText (value >= novi::minSampleBpm ? juce::String (value, 1) : juce::String(), false);
    editor.setVisible (true);
    editor.grabKeyboardFocus();
    editor.selectAll();
}

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

namespace layout
{
    constexpr int r1 = 446, r2 = 612, rowH = 150;
}

FacePanel::FacePanel (NoviSliceProcessor& p) : proc (p)
{
    using namespace layout;
    setLookAndFeel (&look);
    setOpaque (true);

    addAndMakeVisible (lcd);
    addAndMakeVisible (bpmBox);
    lcd.onSliceClicked = [this] (int slice) { selectPad (proc.getKeyForSlice (slice)); };
    lcd.onPadEdited = [this] { loadPadControls(); };

    for (auto* b : { &loadButton, &playButton })
        addAndMakeVisible (*b);
    resetButton.setButtonText ("RESET ORDER");
    addToPage (shuffleButton, pagePlay);
    addToPage (resetButton, pagePlay);
    playButton.setClickingTogglesState (true);
    playButton.setTooltip ("Play the loaded sample from start to finish (between the trim points). Click again to stop.");
    playButton.onClick = [this]
    {
        if (! proc.hasSample()) { playButton.setToggleState (false, juce::dontSendNotification); lcd.flash ("LOAD A LOOP FIRST"); return; }
        playPressedAt = juce::Time::getMillisecondCounter();
        proc.requestPreview (playButton.getToggleState());
    };
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

    static const char* tabNames[numPages] = { "SOUND", "CHOP", "PAD EDIT", "PLAY MODES" };
    for (int i = 0; i < numPages; ++i)
    {
        auto& t = pageTabs[(size_t) i];
        t.setButtonText (tabNames[i]);
        t.setRadioGroupId (88);
        t.setClickingTogglesState (true);
        t.onClick = [this, i] { if (pageTabs[(size_t) i].getToggleState()) setPage (i); };
        addAndMakeVisible (t);
    }
    pageTabs[0].setToggleState (true, juce::dontSendNotification);

    // ---- always visible: the control strip ----
    addChoice ("bars", {}, NoviSliceProcessor::barsChoices, -1);
    addChoice ("slices", {}, NoviSliceProcessor::slicesChoices, -1);
    addToggle ("triplet", "TRIPLET", -1);
    addToggle ("full", "FULL", -1);
    addToggle ("loop", "LOOP", -1);
    addToggle ("sync", "SYNC", -1);
    addChoice ("stretch", {}, NoviSliceProcessor::stretchChoices, -1).setTooltip ("BEATS: slices play untouched and only their timing follows FL's tempo (best for drums). "
                                 "TONAL: a spectral stretcher that keeps everything smooth (best for chords, vocals, melodic loops).");

    // ---- SOUND ----
    addKnob ("texAmount", "AMOUNT", pageSound);
    addKnob ("texSize", "GRAIN", pageSound);
    addKnob ("vinyl", "VINYL", pageSound);
    addKnob ("pitch", "PITCH", pageSound);
    addKnob ("glide", "GLIDE", pageSound);
    addKnob ("xfade", "X-FADE", pageSound);
    addKnob ("cutoff", "CUTOFF", pageSound);
    addKnob ("res", "RESO", pageSound);
    addKnob ("lfoDepth", "LFO DEPTH", pageSound);
    addChoice ("mode", "MODE", NoviSliceProcessor::modeChoices, pageSound);
    addChoice ("lfoRate", "LFO RATE", NoviSliceProcessor::lfoChoices, pageSound);
    addKnob ("bits", "BITS", pageSound);
    addKnob ("downsample", "RATE", pageSound);
    addKnob ("dlyMix", "MIX", pageSound);
    addKnob ("dlyFb", "FEEDBACK", pageSound);
    addChoice ("dlyTime", "TIME", NoviSliceProcessor::delayChoices, pageSound);
    addKnob ("attack", "ATTACK", pageSound);
    addKnob ("release", "RELEASE", pageSound);
    addKnob ("gain", "LEVEL", pageSound);

    // ---- CHOP ----
    addChoice ("chopMode", "MODE", NoviSliceProcessor::chopChoices, pageChop);
    addToggle ("zeroSnap", "0 SNAP", pageChop);
    addToggle ("linkSlices", "LINK", pageChop);
    addKnob ("regions", "REGIONS", pageChop);
    addKnob ("sensitivity", "SENSITIVITY", pageChop);
    addKnob ("minTime", "MIN TIME", pageChop);
    for (auto id : { "trimStart", "trimEnd" })
    {
        auto& k = addKnob (id, juce::String (id) == "trimStart" ? "START" : "END", pageChop);
        k.textFromValueFunction = [this] (double v)
        {
            return proc.hasSample() ? juce::String (v * proc.getSampleSeconds(), 2) + " s" : juce::String (v * 100.0, 1) + " %";
        };
        k.setMouseDragSensitivity (600);
    }
    addButton ("SPLIT", pageChop, [this] { if (lcd.selectedSlice >= 0) proc.splitSlice (lcd.selectedSlice); lcd.refresh(); });
    addButton ("COMBINE", pageChop, [this] { if (lcd.selectedSlice > 0) proc.combineSlice (lcd.selectedSlice); lcd.refresh(); });
    addButton ("CLEAR", pageChop, [this] { proc.clearSlices(); lcd.refresh(); });
    addButton ("NORMALIZE", pageChop, [this] { proc.normalizeSample(); lcd.flash ("NORMALIZED"); });
    addButton ("REVERSE", pageChop, [this] { proc.reverseSample(); lcd.flash ("REVERSED"); });
    addButton ("TO MONO", pageChop, [this]
    {
        juce::PopupMenu m;
        m.addItem (1, "Mono: sum left + right");
        m.addItem (2, "Mono: left channel only");
        m.addItem (3, "Mono: right channel only");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (byId["TO MONO"]),
                         [this] (int r) { if (r > 0) { proc.sampleToMono (r - 1); lcd.flash ("MONO"); } });
    });
    addButton ("EXPORT WAVS", pageChop, [this] { exportWavs(); });
    dragMidi = std::make_unique<DragFileButton> ("DRAG MIDI", [this] { return proc.writeMidiPatternToTempFile(); });
    dragMidi->setTooltip ("Drag this into FL Studio's piano roll or playlist: a MIDI pattern that plays the slices in order.");
    dragMidi->onClick = [this] { lcd.flash ("DRAG THE BUTTON INTO FL STUDIO"); };
    addToPage (*dragMidi, pageChop);
    byId["DRAG MIDI"] = dragMidi.get();

    // ---- PAD EDIT ----
    padTune = &addPadKnob ("TUNE", -24.0, 24.0, 0.01, 0.0, 0.0,
                           [] (double v) { return (v > 0.004 ? "+" : "") + juce::String (v, std::abs (v - std::round (v)) < 0.005 ? 0 : 2) + " st"; });
    padLevel = &addPadKnob ("LEVEL", -48.0, 6.0, 0.1, -9.0, 0.0,
                            [] (double v) { return v <= -47.9 ? juce::String ("-INF") : (v > 0.05 ? "+" : "") + juce::String (v, 1) + " dB"; });
    padPan = &addPadKnob ("PAN", -1.0, 1.0, 0.01, 0.0, 0.0,
                          [] (double v) { const int p = juce::roundToInt (v * 50); return p == 0 ? juce::String ("C") : (p < 0 ? "L" : "R") + juce::String (std::abs (p)); });
    padCutoff = &addPadKnob ("CUTOFF", 20.0, 20000.0, 1.0, 1000.0, 20000.0,
                             [] (double v) { return v >= 19950.0 ? juce::String ("OPEN") : v >= 1000.0 ? juce::String (v / 1000.0, 1) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz"; });
    padTail = &addPadKnob ("TAIL", 0.0, 1000.0, 1.0, 150.0, 0.0,
                           [] (double v) { return v < 0.5 ? juce::String ("OFF") : juce::String (juce::roundToInt (v)) + " ms"; });
    padStart = &addPadKnob ("START", -250.0, 250.0, 0.1, 0.0, 0.0,
                            [] (double v) { return std::abs (v) < 0.05 ? juce::String ("0 ms") : (v > 0 ? "+" : "") + juce::String (v, 1) + " ms"; });
    padEnd = &addPadKnob ("END", -250.0, 250.0, 0.1, 0.0, 0.0,
                          [] (double v) { return std::abs (v) < 0.05 ? juce::String ("0 ms") : (v > 0 ? "+" : "") + juce::String (v, 1) + " ms"; });
    padFadeIn = &addPadKnob ("FADE IN", 0.0, 200.0, 0.1, 20.0, 0.0,
                             [] (double v) { return v < 0.05 ? juce::String ("OFF") : juce::String (v, v < 10.0 ? 1 : 0) + " ms"; });
    padFadeOut = &addPadKnob ("FADE OUT", 0.0, 500.0, 0.1, 40.0, 0.0,
                              [] (double v) { return v < 0.05 ? juce::String ("X-FADE") : juce::String (v, v < 10.0 ? 1 : 0) + " ms"; });
    for (auto* k : { padStart, padEnd, padFadeIn, padFadeOut })
    {
        k->setMouseDragSensitivity (900);   // fine control; or click the value and type it
        k->valueFromTextFunction = [] (const juce::String& t) { return t.retainCharacters ("-+0123456789.").getDoubleValue(); };
        for (auto* child : k->getChildren())
            if (auto* label = dynamic_cast<juce::Label*> (child))
            {
                label->setEditable (true, true, false);
                label->setInterceptsMouseClicks (true, true);
            }
    }
    addButton ("RESET S/E", pagePad, [this]
    {
        auto pd = proc.getPad (selectedPad);
        pd.startMs = pd.endMs = pd.fadeInMs = pd.fadeOutMs = 0.f;
        proc.setPad (selectedPad, pd);
        loadPadControls();
    });
    padReverse = &addPadToggle ("REVERSE");
    padLoop = &addPadToggle ("LOOP");
    padMute = &addPadToggle ("MUTE");
    padSolo = &addPadToggle ("SOLO");
    addButton ("RESET PAD", pagePad, [this] { proc.setPad (selectedPad, {}); loadPadControls(); });
    addButton ("COPY TO ALL", pagePad, [this]
    {
        const auto src = proc.getPad (selectedPad);
        for (int k = 0; k < novi::maxSlices; ++k)
        {
            auto pd = src;
            const auto old = proc.getPad (k);
            pd.mute = old.mute; pd.solo = old.solo;   // mutes and solos stay per pad
            proc.setPad (k, pd);
        }
        lcd.flash ("PAD " + juce::String (selectedPad + 1) + " COPIED TO ALL");
    });
    padPlayMode = &addPadChoice ("PLAY MODE", { "NOTE ON", "ONE SHOT", "NOTE OFF" });
    padMuteGroup = &addPadChoice ("MUTE GROUP", { "OFF", "1", "2", "3", "4", "5", "6", "7", "8" });
    {
        juce::StringArray reps { "HOLD" };
        for (int i = 1; i <= 16; ++i) reps.add (juce::String (i) + "X");
        padRepeats = &addPadChoice ("REPEATS", reps);
    }
    padMotion = &addPadChoice ("MOTION", { "OFF", "NEXT", "RANDOM" });
    {
        juce::StringArray cyc { "ALL" };
        for (int i = 2; i <= novi::maxSlices; ++i) cyc.add (juce::String (i));
        padCycles = &addPadChoice ("CYCLES", cyc);
    }

    // ---- PLAY MODES ----
    addChoice ("poly", "POLYPHONY", NoviSliceProcessor::polyChoices, pagePlay);
    addToggle ("fullLevel", "FULL LEVEL", pagePlay);
    addKnob ("velSens", "VEL SENS", pagePlay);
    addToggle ("noteRepeat", "REPEAT", pagePlay);
    addChoice ("repeatRate", "RATE", NoviSliceProcessor::repeatChoices, pagePlay);
    addKnob ("swing", "SWING", pagePlay);
    auto& sixteen = addToggle ("sixteen", "16 LEVELS", pagePlay);
    sixteen.onClick = [this, &sixteen]
    {
        if (sixteen.getToggleState())
            if (auto* prm = proc.apvts.getParameter ("sixteenSource"))
                prm->setValueNotifyingHost (prm->convertTo0to1 ((float) (selectedPad + 1)));
    };
    addChoice ("sixteenType", "TYPE", NoviSliceProcessor::sixteenChoices, pagePlay);
    addButton ("USE SELECTED PAD", pagePlay, [this]
    {
        if (auto* prm = proc.apvts.getParameter ("sixteenSource"))
            prm->setValueNotifyingHost (prm->convertTo0to1 ((float) (selectedPad + 1)));
    });
    addButton ("CLEAR MUTES", pagePlay, [this] { proc.clearPadFlags (true, false); loadPadControls(); });
    addButton ("CLEAR SOLOS", pagePlay, [this] { proc.clearPadFlags (false, true); loadPadControls(); });

    // ---- pads and perform ----
    for (auto& pad : pads)
    {
        pad = std::make_unique<PadButton> (proc, fonts);
        pad->onSelect = [this] (int key) { selectPad (key); };
        addAndMakeVisible (*pad);
    }
    for (int i = 0; i < novi::numPerfFx; ++i)
    {
        perfButtons[(size_t) i] = std::make_unique<PerfButton> (proc, fonts, i);
        addAndMakeVisible (*perfButtons[(size_t) i]);
    }

    lastGridSwitch = proc.gridSwitchCount.load();
    setSize (designW, designH);
    setPage (pageSound);
    loadPadControls();
    lcd.refresh();
}

FacePanel::~FacePanel()
{
    sliderAttachments.clear();
    comboAttachments.clear();
    buttonAttachments.clear();
    setLookAndFeel (nullptr);
}

void FacePanel::addToPage (juce::Component& c, int pg, const juce::String& label)
{
    addAndMakeVisible (c);
    pageMembers.push_back ({ &c, pg });
    if (label.isNotEmpty())
        labelled.push_back ({ &c, label, pg });
}

juce::Slider& FacePanel::addKnob (const juce::String& id, const juce::String& label, int pg)
{
    auto s = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
    s->setTextBoxStyle (juce::Slider::TextBoxBelow, true, 70, 16);
    s->setRotaryParameters (juce::degreesToRadians (225.f), juce::degreesToRadians (495.f), true);
    s->setMouseDragSensitivity (220);
    s->setColour (juce::Slider::textBoxTextColourId, col::ink);
    s->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    s->setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, *s));
    if (auto* param = proc.apvts.getParameter (id))
        s->setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    addToPage (*s, pg, label);
    byId[id] = s.get();
    sliders.push_back (std::move (s));
    return *sliders.back();
}

juce::ComboBox& FacePanel::addChoice (const juce::String& id, const juce::String& label, const juce::StringArray& items, int pg)
{
    auto c = std::make_unique<juce::ComboBox>();
    c->addItemList (items, 1);
    comboAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, *c));
    addToPage (*c, pg, label);
    byId[id] = c.get();
    combos.push_back (std::move (c));
    return *combos.back();
}

juce::TextButton& FacePanel::addToggle (const juce::String& id, const juce::String& text, int pg)
{
    auto b = std::make_unique<juce::TextButton> (text);
    b->setClickingTogglesState (true);
    buttonAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, id, *b));
    addToPage (*b, pg);
    byId[id] = b.get();
    auto* raw = b.get();
    buttons.push_back (std::move (b));
    return *raw;
}

juce::TextButton& FacePanel::addButton (const juce::String& text, int pg, std::function<void()> onClick)
{
    auto b = std::make_unique<juce::TextButton> (text);
    b->onClick = std::move (onClick);
    addToPage (*b, pg);
    byId[text] = b.get();
    auto* raw = b.get();
    buttons.push_back (std::move (b));
    return *raw;
}

juce::Slider& FacePanel::addPadKnob (const juce::String& label, double lo, double hi, double step, double centre, double def,
                                     std::function<juce::String (double)> text)
{
    auto s = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
    s->setTextBoxStyle (juce::Slider::TextBoxBelow, true, 70, 16);
    s->setRotaryParameters (juce::degreesToRadians (225.f), juce::degreesToRadians (495.f), true);
    s->setMouseDragSensitivity (220);
    s->setRange (lo, hi, step);
    if (centre > lo && centre < hi) s->setSkewFactorFromMidPoint (centre);
    s->setDoubleClickReturnValue (true, def);
    s->setValue (def, juce::dontSendNotification);
    s->textFromValueFunction = std::move (text);
    s->setColour (juce::Slider::textBoxTextColourId, col::ink);
    s->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    s->setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    s->onValueChange = [this] { storePadControls(); };
    s->updateText();
    addToPage (*s, pagePad, label);
    byId["pad " + label] = s.get();
    sliders.push_back (std::move (s));
    return *sliders.back();
}

juce::ComboBox& FacePanel::addPadChoice (const juce::String& label, const juce::StringArray& items)
{
    auto c = std::make_unique<juce::ComboBox>();
    c->addItemList (items, 1);
    c->setSelectedItemIndex (0, juce::dontSendNotification);
    c->onChange = [this] { storePadControls(); };
    addToPage (*c, pagePad, label);
    byId["pad " + label] = c.get();
    combos.push_back (std::move (c));
    return *combos.back();
}

juce::TextButton& FacePanel::addPadToggle (const juce::String& text)
{
    auto b = std::make_unique<juce::TextButton> (text);
    b->setClickingTogglesState (true);
    b->onClick = [this] { storePadControls(); };
    addToPage (*b, pagePad);
    byId["pad " + text] = b.get();
    auto* raw = b.get();
    buttons.push_back (std::move (b));
    return *raw;
}

void FacePanel::place (juce::Component& c, int x, int y, int w, int h) { c.setBounds (x, y, w, h); }

void FacePanel::loadPadControls()
{
    const auto pd = proc.getPad (selectedPad);
    loadingPad = true;
    padTune->setValue (pd.tune, juce::dontSendNotification);
    padLevel->setValue (pd.levelDb, juce::dontSendNotification);
    padPan->setValue (pd.pan, juce::dontSendNotification);
    padCutoff->setValue (pd.cutoffHz, juce::dontSendNotification);
    padTail->setValue (pd.tailMs, juce::dontSendNotification);
    padStart->setValue (pd.startMs, juce::dontSendNotification);
    padEnd->setValue (pd.endMs, juce::dontSendNotification);
    padFadeIn->setValue (pd.fadeInMs, juce::dontSendNotification);
    padFadeOut->setValue (pd.fadeOutMs, juce::dontSendNotification);
    for (auto* k : { padStart, padEnd, padFadeIn, padFadeOut }) k->updateText();
    for (auto* s : { padTune, padLevel, padPan, padCutoff, padTail }) s->updateText();
    padReverse->setToggleState (pd.reverse, juce::dontSendNotification);
    padLoop->setToggleState (pd.loop, juce::dontSendNotification);
    padMute->setToggleState (pd.mute, juce::dontSendNotification);
    padSolo->setToggleState (pd.solo, juce::dontSendNotification);
    padPlayMode->setSelectedItemIndex (pd.playMode, juce::dontSendNotification);
    padMuteGroup->setSelectedItemIndex (pd.muteGroup, juce::dontSendNotification);
    padRepeats->setSelectedItemIndex (juce::jlimit (0, 16, pd.repeats), juce::dontSendNotification);
    padMotion->setSelectedItemIndex (pd.motion, juce::dontSendNotification);
    padCycles->setSelectedItemIndex (pd.cycles <= 1 ? 0 : pd.cycles - 1, juce::dontSendNotification);
    loadingPad = false;
}

void FacePanel::storePadControls()
{
    if (loadingPad || padTune == nullptr || padCycles == nullptr || padFadeOut == nullptr)
        return;
    novi::PadSettings pd = proc.getPad (selectedPad);
    pd.tune = (float) padTune->getValue();
    pd.levelDb = (float) padLevel->getValue();
    pd.pan = (float) padPan->getValue();
    pd.cutoffHz = padCutoff->getValue() >= 19950.0 ? 20000.f : (float) padCutoff->getValue();
    pd.tailMs = (float) padTail->getValue();
    pd.startMs = (float) padStart->getValue();
    pd.endMs = (float) padEnd->getValue();
    pd.fadeInMs = (float) padFadeIn->getValue();
    pd.fadeOutMs = (float) padFadeOut->getValue();
    pd.reverse = padReverse->getToggleState();
    pd.loop = padLoop->getToggleState();
    pd.mute = padMute->getToggleState();
    pd.solo = padSolo->getToggleState();
    pd.playMode = juce::jmax (0, padPlayMode->getSelectedItemIndex());
    pd.muteGroup = juce::jmax (0, padMuteGroup->getSelectedItemIndex());
    pd.repeats = juce::jmax (0, padRepeats->getSelectedItemIndex());
    pd.motion = juce::jmax (0, padMotion->getSelectedItemIndex());
    const int ci = padCycles->getSelectedItemIndex();
    pd.cycles = ci <= 0 ? 0 : ci + 1;
    proc.setPad (selectedPad, pd);
}

void FacePanel::selectPad (int key)
{
    key = juce::jlimit (0, novi::maxSlices - 1, key);
    if (key == selectedPad) return;
    selectedPad = key;
    if (key >= 16 && bank == 0) { bank = 1; bankB.setToggleState (true, juce::dontSendNotification); }
    if (key < 16 && bank == 1) { bank = 0; bankA.setToggleState (true, juce::dontSendNotification); }
    loadPadControls();
    repaint (Rectangle<int> (420, 396, 310, 34));
}

void FacePanel::setPage (int pg)
{
    page = juce::jlimit (0, numPages - 1, pg);
    for (auto& [comp, p] : pageMembers)
        comp->setVisible (p < 0 || p == page);
    pageTabs[(size_t) page].setToggleState (true, juce::dontSendNotification);
    lcd.editing = page == pageChop;
    if (page == pagePad) loadPadControls();
    chassisCache = {};
    repaint();
}

void FacePanel::resized()
{
    using namespace layout;
    sections.clear();
    lcd.setBounds (24, 74, 704, 240);

    // control strip
    {
        int x = 24;
        const int y = 344, h = 34;
        auto next = [&] (juce::Component& c, int w, int gap = 8) { c.setBounds (x, y, w, h); x += w + gap; };
        next (loadButton, 52);
        next (playButton, 54);
        bpmBox.setBounds (x, y + 4, 70, 26); x += 70 + 8;
        byId["bars"]->setBounds (x, y + 4, 48, 26); x += 48 + 6;
        byId["slices"]->setBounds (x, y + 4, 48, 26); x += 48 + 10;
        next (*byId["triplet"], 60);
        next (*byId["full"], 50);
        next (*byId["loop"], 50);
        next (*byId["sync"], 52, 12);
        byId["stretch"]->setBounds (x, y + 4, 728 - x, 26);
    }

    for (int i = 0; i < numPages; ++i)
        pageTabs[(size_t) i].setBounds (24 + i * 100, 400, 94, 32);

    auto section = [this] (const char* title, int x, int y, int w, int h, int pg) { sections.push_back ({ title, { x, y, w, h }, pg }); };
    auto knob = [this] (const juce::String& id, int x, int y) { placeKnob (*byId[id], x, y); };

    // SOUND
    section ("TEXTURE", 24, r1, 204, rowH, pageSound);
    knob ("texAmount", 32, r1 + 26); knob ("texSize", 96, r1 + 26); knob ("vinyl", 160, r1 + 26);
    section ("PLAY", 236, r1, 204, rowH, pageSound);
    knob ("pitch", 244, r1 + 26); knob ("glide", 308, r1 + 26); knob ("xfade", 372, r1 + 26);
    section ("FILTER", 448, r1, 280, rowH, pageSound);
    knob ("cutoff", 456, r1 + 26); knob ("res", 520, r1 + 26); knob ("lfoDepth", 584, r1 + 26);
    placeLabelled (*byId["mode"], 652, r1 + 26, 66);
    placeLabelled (*byId["lfoRate"], 652, r1 + 82, 66);
    section ("CRUSH", 24, r2, 140, rowH, pageSound);
    knob ("bits", 32, r2 + 26); knob ("downsample", 96, r2 + 26);
    section ("DELAY", 172, r2, 216, rowH, pageSound);
    knob ("dlyMix", 180, r2 + 26); knob ("dlyFb", 244, r2 + 26);
    placeLabelled (*byId["dlyTime"], 312, r2 + 26, 66);
    section ("AMP ENV", 396, r2, 140, rowH, pageSound);
    knob ("attack", 404, r2 + 26); knob ("release", 468, r2 + 26);
    section ("OUTPUT", 544, r2, 184, rowH, pageSound);
    knob ("gain", 552, r2 + 26);

    // CHOP
    section ("CHOP MODE", 24, r1, 196, rowH, pageChop);
    placeLabelled (*byId["chopMode"], 36, r1 + 26, 172, 28);
    place (*byId["zeroSnap"], 36, r1 + 90, 82, 36);
    place (*byId["linkSlices"], 126, r1 + 90, 82, 36);
    section ("DETECT", 228, r1, 204, rowH, pageChop);
    knob ("regions", 236, r1 + 26); knob ("sensitivity", 300, r1 + 26); knob ("minTime", 364, r1 + 26);
    section ("TRIM", 440, r1, 140, rowH, pageChop);
    knob ("trimStart", 448, r1 + 26); knob ("trimEnd", 512, r1 + 26);
    section ("SLICES", 588, r1, 140, rowH, pageChop);
    place (*byId["SPLIT"], 600, r1 + 22, 116, 32);
    place (*byId["COMBINE"], 600, r1 + 60, 116, 32);
    place (*byId["CLEAR"], 600, r1 + 98, 116, 32);
    section ("PROCESS", 24, r2, 300, rowH, pageChop);
    place (*byId["NORMALIZE"], 36, r2 + 30, 88, 36);
    place (*byId["REVERSE"], 130, r2 + 30, 88, 36);
    place (*byId["TO MONO"], 224, r2 + 30, 88, 36);
    section ("EXPORT", 332, r2, 396, rowH, pageChop);
    place (*byId["EXPORT WAVS"], 344, r2 + 30, 180, 36);
    place (*dragMidi, 536, r2 + 30, 180, 36);

    // PAD EDIT
    section ("PAD SOUND", 24, r1, 340, rowH, pagePad);
    placeKnob (*padTune, 32, r1 + 26); placeKnob (*padLevel, 96, r1 + 26); placeKnob (*padPan, 160, r1 + 26);
    placeKnob (*padCutoff, 224, r1 + 26); placeKnob (*padTail, 288, r1 + 26);
    section ("START / END", 372, r1, 356, rowH, pagePad);
    placeKnob (*padStart, 380, r1 + 26); placeKnob (*padEnd, 444, r1 + 26);
    placeKnob (*padFadeIn, 508, r1 + 26); placeKnob (*padFadeOut, 572, r1 + 26);
    place (*byId["RESET S/E"], 642, r1 + 52, 78, 30);
    section ("TRIGGER", 24, r2, 340, rowH, pagePad);
    placeLabelled (*padPlayMode, 36, r2 + 26, 116, 28);
    placeLabelled (*padMuteGroup, 160, r2 + 26, 84, 28);
    placeLabelled (*padRepeats, 252, r2 + 26, 100, 28);
    section ("SWITCHES / MOTION", 372, r2, 356, rowH, pagePad);
    place (*padReverse, 384, r2 + 22, 80, 32);
    place (*padLoop, 470, r2 + 22, 80, 32);
    place (*padMute, 556, r2 + 22, 76, 32);
    place (*padSolo, 638, r2 + 22, 80, 32);
    placeLabelled (*padMotion, 384, r2 + 62, 100, 26);
    placeLabelled (*padCycles, 490, r2 + 62, 70, 26);
    place (*byId["RESET PAD"], 568, r2 + 78, 72, 26);
    place (*byId["COPY TO ALL"], 644, r2 + 78, 76, 26);

    // PLAY MODES
    section ("VOICES", 24, r1, 270, rowH, pagePlay);
    placeLabelled (*byId["poly"], 36, r1 + 26, 104, 28);
    place (*byId["fullLevel"], 36, r1 + 90, 104, 36);
    knob ("velSens", 168, r1 + 26);
    section ("NOTE REPEAT", 302, r1, 426, rowH, pagePlay);
    place (*byId["noteRepeat"], 314, r1 + 42, 90, 36);
    placeLabelled (*byId["repeatRate"], 416, r1 + 26, 84, 28);
    knob ("swing", 514, r1 + 26);
    section ("16 LEVELS", 24, r2, 452, rowH, pagePlay);
    place (*byId["sixteen"], 36, r2 + 42, 100, 36);
    placeLabelled (*byId["sixteenType"], 148, r2 + 26, 116, 28);
    place (*byId["USE SELECTED PAD"], 276, r2 + 42, 188, 36);
    section ("PADS", 484, r2, 244, rowH, pagePlay);
    place (*byId["CLEAR MUTES"], 496, r2 + 26, 108, 34);
    place (*byId["CLEAR SOLOS"], 610, r2 + 26, 108, 34);
    place (shuffleButton, 496, r2 + 68, 108, 34);
    place (resetButton, 610, r2 + 68, 108, 34);

    // pads
    sections.push_back ({ "PADS", { 744, 74, 336, 372 }, -1 });
    bankA.setBounds (982, 78, 44, 30);
    bankB.setBounds (1030, 78, 44, 30);
    const int padSize = 74, gap = 6, gx = 752, gy = 120;
    for (int i = 0; i < 16; ++i)
    {
        const int row = 3 - i / 4, c = i % 4;   // pad 1 bottom-left, like the classic samplers
        pads[(size_t) i]->setBounds (gx + c * (padSize + gap), gy + row * (padSize + gap), padSize, padSize);
    }

    sections.push_back ({ "PERFORM", { 744, 458, 336, 304 }, -1 });
    const int py = 492, ph = 60;
    for (int i = 0; i < novi::numPerfFx; ++i)
        perfButtons[(size_t) i]->setBounds (gx + (i % 4) * (padSize + gap), py + (i / 4) * (ph + 6), padSize, ph);

    chassisCache = {};
}

void FacePanel::paintChassis (juce::Graphics& g)
{
    using namespace layout;
    const auto b = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (col::chassisTop, 0.f, 0.f, col::chassisBot, 0.f, b.getHeight(), false));
    g.fillAll();

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

    for (int i = 0; i < 18; ++i)
    {
        auto slot = Rectangle<float> (836.f + i * 13.f, 22.f, 6.f, 26.f);
        g.setColour (Colour (0xff1e1c1a));
        g.fillRoundedRectangle (slot, 3.f);
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.drawLine (slot.getX() + 1.f, slot.getBottom() + 1.f, slot.getRight() - 1.f, slot.getBottom() + 1.f, 1.f);
    }

    g.setColour (col::accent);
    g.fillRect (0.f, 60.f, b.getWidth(), 3.f);
    g.setColour (col::ink);
    g.fillRect (0.f, 64.f, b.getWidth(), 1.5f);

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
    g.drawText ("LOOP BPM", bpmBox.getBounds().translated (0, 28).withHeight (12), juce::Justification::centred, false);
    g.drawText ("BARS", byId["bars"]->getBounds().translated (0, 28).withHeight (12), juce::Justification::centred, false);
    g.drawText ("PER BAR", byId["slices"]->getBounds().translated (0, 28).withHeight (12), juce::Justification::centred, false);
    g.drawText ("STRETCH", byId["stretch"]->getBounds().translated (0, 28).withHeight (12), juce::Justification::centred, false);
    g.drawText ("PLAYBACK", byId["triplet"]->getBounds().getUnion (byId["sync"]->getBounds()).translated (0, 38).withHeight (12),
                juce::Justification::centred, false);

    // sections for this page
    for (auto& s : sections)
    {
        if (s.page >= 0 && s.page != page) continue;
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

    // control labels for this page
    g.setFont (fonts.semi (12.5f, 0.1f));
    g.setColour (col::ink);
    for (auto& l : labelled)
        if (l.page < 0 || l.page == page)
            g.drawText (l.label, l.comp->getBounds().translated (-10, -16).withWidth (l.comp->getWidth() + 20).withHeight (14), juce::Justification::centred, false);

    // hints
    g.setFont (fonts.mono (10.5f));
    g.setColour (col::printLight);
    auto hint = [&] (const juce::String& text, int x, int y, int w) { g.drawText (text, x, y, w, 14, juce::Justification::centredLeft, false); };
    hint ("KEYS FROM C5  -  DRAG A PAD OUT", 754, 92, 226);
    hint ("HOLD TO PLAY  -  SHIFT-CLICK TO LATCH  -  KEYS C3-D#4", 752, 470, 320);
    if (page == pageChop)
        hint ("DRAG MARKERS TO MOVE  -  DOUBLE-CLICK TO ADD  -  ALT-CLICK TO DELETE  -  DRAG S / E TO TRIM", 24, 318, 704);
    else if (page == pagePad)
        hint ("ZOOMED ON THE SELECTED PAD  -  DRAG S / E TO FINE-TUNE ITS START AND END  -  CLICK TO HEAR IT", 24, 318, 704);
    else
        hint ("CLICK THE SCREEN TO AUDITION  -  DROP A FILE ON IT TO LOAD  -  DOUBLE-CLICK LOOP BPM TO TYPE A TEMPO", 24, 318, 704);

    if (page == pageChop)
    {
        hint ("MOVING A MARKER = MANUAL", 36, r1 + 132, 180);
        hint ("THESE CHANGE THE SAMPLE ITSELF", 36, r2 + 80, 280);
        hint ("(SAVED WITH YOUR PROJECT)", 36, r2 + 96, 280);
        hint ("EXPORT: EVERY SLICE AS ITS OWN WAV FILE", 344, r2 + 80, 380);
        hint ("DRAG MIDI: INTO FL'S PIANO ROLL - PLAYS THE SLICES IN ORDER", 344, r2 + 96, 380);
        hint ("OR DRAG ANY PAD OUT OF NOVISLICE TO DROP ITS SLICE", 344, r2 + 112, 380);
    }
    if (page == pagePad)
    {
        hint ("NOTE ON: PLAYS WHILE HELD  -  ONE SHOT: WHOLE SLICE", 36, r2 + 84, 320);
        hint ("NOTE OFF: PLAYS WHEN RELEASED", 36, r2 + 100, 320);
        hint ("MUTE GROUP: PADS IN A GROUP CUT EACH OTHER OFF", 36, r2 + 116, 320);
        hint ("REPEATS: HOW MANY TIMES A LOOPED PAD PLAYS", 36, r2 + 132, 320);
        hint ("MOTION: EACH HIT PLAYS THE NEXT / A RANDOM SLICE", 384, r2 + 116, 336);
        hint ("LOOP NEEDS PLAY MODE = NOTE ON", 384, r2 + 132, 336);
        hint ("CLICK VALUES", 642, r1 + 88, 84);
        hint ("TO TYPE THEM,", 642, r1 + 104, 84);
        hint ("OR DRAG S / E", 642, r1 + 120, 84);
    }
    if (page == pagePlay)
    {
        hint ("MONO: A NEW PAD CUTS THE LAST ONE", 36, r1 + 132, 250);
        hint ("HOLD A PAD TO", 596, r1 + 34, 130);
        hint ("RETRIGGER IT IN", 596, r1 + 50, 130);
        hint ("TIME WITH FL", 596, r1 + 66, 130);
        hint ("SWING: 1/4-1/32,", 596, r1 + 90, 130);
        hint ("NOT TRIPLETS", 596, r1 + 106, 130);
        hint ("ONE PAD SPREAD OVER PADS 1-16 AT RISING LEVELS", 36, r2 + 96, 430);
        hint ("TUNE: PAD 9 = ORIGINAL PITCH", 36, r2 + 112, 430);
        hint ("SHUFFLE: PADS PLAY SLICES IN A", 496, r2 + 112, 230);
        hint ("RANDOM ORDER (MUTE/SOLO: PAD EDIT)", 496, r2 + 128, 230);
    }

    if (page == pageSound)
    {
        auto meter = Rectangle<float> (636.f, (float) meterY, 18.f, 110.f);
        g.setColour (col::bezel);
        g.fillRoundedRectangle (meter.expanded (4.f), 4.f);
        g.setColour (col::print);
        g.setFont (fonts.mono (10.f));
        g.drawText ("+0", Rectangle<float> (meter.getRight() + 8.f, meter.getY() - 2.f, 30.f, 12.f), juce::Justification::centredLeft, false);
        g.drawText ("-12", Rectangle<float> (meter.getRight() + 8.f, meter.getCentreY() - 6.f, 30.f, 12.f), juce::Justification::centredLeft, false);
        g.drawText ("-48", Rectangle<float> (meter.getRight() + 8.f, meter.getBottom() - 10.f, 30.f, 12.f), juce::Justification::centredLeft, false);
    }
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
    // selected pad readout next to the page tabs
    {
        auto area = Rectangle<float> (430.f, 400.f, 298.f, 32.f);
        g.setColour (col::bezel);
        g.fillRoundedRectangle (area, 4.f);
        auto glass = area.reduced (3.f);
        g.setGradientFill (juce::ColourGradient (col::lcdTop, 0.f, glass.getY(), col::lcdBot, 0.f, glass.getBottom(), false));
        g.fillRoundedRectangle (glass, 2.f);
        g.setColour (col::lcdInk);
        g.setFont (fonts.mono (14.f));
        const int total = proc.getNumSlices();
        juce::String t = "PAD " + juce::String (selectedPad + 1).paddedLeft ('0', 2);
        if (selectedPad < total) t << "  SLICE " << juce::String (proc.getSliceForKey (selectedPad) + 1).paddedLeft ('0', 2);
        else t << "  NO SLICE";
        t << "  " << noteName (novi::noteForSliceKey (selectedPad));
        if (page == pagePlay && proc.apvts.getRawParameterValue ("sixteen")->load() > 0.5f)
            t = "16 LEVELS SOURCE: PAD " + juce::String (juce::roundToInt (proc.apvts.getRawParameterValue ("sixteenSource")->load())).paddedLeft ('0', 2);
        g.drawText (t, glass.reduced (8.f, 0.f), juce::Justification::centredLeft, false);
    }

    if (page != pageSound)
        return;
    auto meter = Rectangle<float> (636.f, (float) meterY, 18.f, 110.f);
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
    if (const int gs = proc.gridSwitchCount.load(); gs != lastGridSwitch)
    {
        lastGridSwitch = gs;
        lcd.flash ("CHOP MODE: BPM GRID");
    }
    lcd.refresh();
    const int total = proc.getNumSlices();
    bankB.setEnabled (total > 16 || selectedPad >= 16);
    if (total <= 16 && bank == 1 && selectedPad < 16)
    {
        bank = 0;
        bankA.setToggleState (true, juce::dontSendNotification);
    }
    lcd.selectedSlice = selectedPad < total ? proc.getSliceForKey (selectedPad) : -1;
    lcd.zoomPad = (page == pagePad && selectedPad < total) ? selectedPad : -1;

    const bool sixteen = proc.apvts.getRawParameterValue ("sixteen")->load() > 0.5f;
    const int source = juce::roundToInt (proc.apvts.getRawParameterValue ("sixteenSource")->load()) - 1;
    const int activeKey = proc.engine.uiActiveKey.load();
    const bool loaded = proc.hasSample();
    for (int i = 0; i < 16; ++i)
    {
        const int key = bank * 16 + i;
        const bool levelPad = sixteen && bank == 0;
        pads[(size_t) i]->setState (key, loaded && (levelPad ? source < total : key < total),
                                    key == activeKey, ! levelPad && key == selectedPad, levelPad ? i : -1);
    }
    // PLAY lights while the sample is playing (give the audio thread a moment after a click)
    if (juce::Time::getMillisecondCounter() - playPressedAt > 250)
        playButton.setToggleState (proc.engine.uiPreviewing.load(), juce::dontSendNotification);

    const auto mask = proc.engine.uiPerfMask.load();
    for (int i = 0; i < novi::numPerfFx; ++i)
        perfButtons[(size_t) i]->setLit ((mask >> i) & 1u);

    if (auto* ts = dynamic_cast<juce::Slider*> (byId["trimStart"])) ts->updateText();
    if (auto* te = dynamic_cast<juce::Slider*> (byId["trimEnd"])) te->updateText();

    const float peak = proc.outputPeak.exchange (0.f);
    meterLevel = juce::jmax (peak, meterLevel * 0.85f);
    if (page == pageSound)
        repaint (Rectangle<int> (630, meterY - 6, 30, 124));
    repaint (Rectangle<int> (428, 398, 302, 36));
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

void FacePanel::exportWavs()
{
    if (! proc.hasSample()) { lcd.flash ("LOAD A LOOP FIRST"); return; }
    chooser = std::make_unique<juce::FileChooser> ("Choose a folder for the slices",
                                                   juce::File::getSpecialLocation (juce::File::userMusicDirectory));
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto dir = fc.getResult();
                              if (dir == juce::File()) return;
                              const int n = proc.exportSlicesTo (dir);
                              lcd.flash ("EXPORTED " + juce::String (n) + " SLICES");
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
        c->setSizeLimits (770, 540, 1650, 1158);
    }
    setSize (990, 695);
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
