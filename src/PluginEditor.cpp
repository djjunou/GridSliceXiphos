#include "PluginEditor.h"
#include "BinaryData.h"

//==============================================================================
// WaveformView

void WaveformView::rebuildPeaks()
{
    peaksMin.clear();
    peaksMax.clear();
    cachedWidth = juce::jmax (1, getWidth());
    cachedVersion = proc.getSampleVersion();

    auto snap = proc.currentSample();
    if (snap == nullptr || snap->buffer.getNumSamples() == 0)
        return;

    const int total = snap->buffer.getNumSamples();
    peaksMin.assign ((size_t) cachedWidth, 0.0f);
    peaksMax.assign ((size_t) cachedWidth, 0.0f);

    for (int x = 0; x < cachedWidth; ++x)
    {
        const int start = (int) ((long long) x * total / cachedWidth);
        const int end = (int) std::max ((long long) start + 1, (long long) (x + 1) * total / cachedWidth);
        float mn = 0.0f, mx = 0.0f;
        for (int ch = 0; ch < snap->buffer.getNumChannels(); ++ch)
            for (int i = start; i < end && i < total; ++i)
            {
                const float s = snap->buffer.getSample (ch, i);
                mn = juce::jmin (mn, s);
                mx = juce::jmax (mx, s);
            }
        peaksMin[(size_t) x] = mn;
        peaksMax[(size_t) x] = mx;
    }
}

void WaveformView::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.fillAll (juce::Colour (0xe6050817)); // deep navy, slightly translucent

    auto snap = proc.currentSample();
    if (snap == nullptr)
    {
        g.setColour (pal.accent1.brighter (0.3f));
        g.setFont (juce::Font (16.0f));
        g.drawText ("Drop a sample here, or click \"Load Sample...\"",
                    getLocalBounds(), juce::Justification::centred);
        return;
    }

    if (cachedVersion != proc.getSampleVersion() || cachedWidth != getWidth())
        rebuildPeaks();

    const float mid = bounds.getCentreY();
    const float amp = bounds.getHeight() * 0.46f;

    // Playing / selected slice background
    const int total = snap->buffer.getNumSamples();
    const int count = proc.currentNumSlices();
    auto sliceRect = [&] (int i) {
        const float x1 = (float) proc.sliceStartSamples (i) / (float) total * bounds.getWidth();
        const float x2 = (float) proc.sliceEndSamples (i) / (float) total * bounds.getWidth();
        return juce::Rectangle<float> (x1, 0.0f, juce::jmax (1.0f, x2 - x1), bounds.getHeight());
    };

    const int playing = proc.getCurrentlyPlayingSlice();
    if (playing >= 0 && playing < count)
    {
        g.setColour (pal.accent1.withAlpha (0.33f));
        g.fillRect (sliceRect (playing));
    }
    if (selectedSlice >= 0 && selectedSlice < count && selectedSlice != playing)
    {
        g.setColour (pal.accent2.withAlpha (0.25f));
        g.fillRect (sliceRect (selectedSlice));
    }

    // Waveform: cyan glow pass under a bright core.
    g.setColour (pal.accent1.withAlpha (0.35f));
    for (int x = 0; x < getWidth() && x < (int) peaksMin.size(); ++x)
        g.drawVerticalLine (x, mid + peaksMin[(size_t) x] * amp - 1.0f,
                            mid + peaksMax[(size_t) x] * amp + 1.0f);
    g.setColour (pal.waveCore);
    for (int x = 0; x < getWidth() && x < (int) peaksMin.size(); ++x)
        g.drawVerticalLine (x, mid + peaksMin[(size_t) x] * amp, mid + peaksMax[(size_t) x] * amp);

    // Slice grid: bar lines (every slice that starts a beat group) stronger.
    g.setFont (juce::Font (10.0f));
    for (int i = 0; i < count; ++i)
    {
        const float x = (float) proc.sliceStartSamples (i) / (float) total * bounds.getWidth();
        g.setColour (pal.gridLine.withAlpha (0.8f));
        g.drawVerticalLine ((int) x, 0.0f, bounds.getHeight());

        const float sliceW = bounds.getWidth() / (float) juce::jmax (1, count);
        if (sliceW > 16.0f || i % 4 == 0)
        {
            g.setColour (juce::Colours::white.withAlpha (0.75f));
            g.drawText (juce::String (i + 1), (int) x + 2, 2, 30, 12,
                        juce::Justification::left);
        }
    }

    // Neon frame.
    g.setColour (pal.accent1.withAlpha (0.4f));
    g.drawRect (bounds, 1.0f);
}

void WaveformView::mouseDown (const juce::MouseEvent& e)
{
    auto snap = proc.currentSample();
    if (snap == nullptr || snap->buffer.getNumSamples() == 0)
        return;

    const double frac = juce::jlimit (0.0, 0.999, (double) e.x / (double) getWidth());
    const long long pos = (long long) (frac * snap->buffer.getNumSamples());
    const int count = proc.currentNumSlices();
    for (int i = 0; i < count; ++i)
    {
        if (pos >= proc.sliceStartSamples (i) && pos < proc.sliceEndSamples (i))
        {
            selectedSlice = i;
            proc.auditionSlice (i);
            if (onSliceSelected != nullptr)
                onSliceSelected();
            repaint();
            return;
        }
    }
}

bool WaveformView::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (f.endsWithIgnoreCase (".wav") || f.endsWithIgnoreCase (".aif")
            || f.endsWithIgnoreCase (".aiff") || f.endsWithIgnoreCase (".flac")
            || f.endsWithIgnoreCase (".mp3") || f.endsWithIgnoreCase (".ogg"))
            return true;
    return false;
}

void WaveformView::filesDropped (const juce::StringArray& files, int, int)
{
    if (files.isEmpty())
        return;
    proc.loadSample (juce::File (files[0]));
    selectedSlice = -1;
    repaint();
}

//==============================================================================
// Y2kLookAndFeel

const UiPalette& uiPalette (int index)
{
    using C = juce::Colour;
    static const UiPalette palettes[4] = {
        // 0 Normal: cyan / magenta / violet neon
        { C (0xff4de8ff), C (0xffff4dd8), C (0xff7a5cff), C (0xffffce54),
          C (0xffd9f2ff), C (0xff8df5ff), C (0xffff4dd8) },
        // 1 P-type (protanopia): blue / amber / orange - no red-green cues
        { C (0xff33b5ff), C (0xffffc400), C (0xffff8a00), C (0xffffc400),
          C (0xffe8f4ff), C (0xff7fd4ff), C (0xffffc400) },
        // 2 D-type (deuteranopia): sky blue / yellow / deep blue
        { C (0xff2ec4ff), C (0xffffe14d), C (0xff0072b2), C (0xffffd23f),
          C (0xffeaf7ff), C (0xffaee9ff), C (0xffffe14d) },
        // 3 T-type (tritanopia): pink / green / peach - no blue-yellow cues
        { C (0xffff6b9d), C (0xff00c896), C (0xffff9e6b), C (0xffffce54),
          C (0xffffeef4), C (0xff7dffc4), C (0xffff6b9d) },
    };
    return palettes[juce::jlimit (0, 3, index)];
}

namespace
{
juce::ColourGradient neonGradient (const UiPalette& pal, float x1, float x2, float y)
{
    juce::ColourGradient g (pal.accent1, x1, y, pal.accent2, x2, y, false);
    g.addColour (0.5, pal.accent3);
    return g;
}

juce::ColourGradient chromeGradient (float x, float y1, float y2)
{
    juce::ColourGradient g (juce::Colour (0xfffdfdff), x, y1,
                            juce::Colour (0xff7db8ff), x, y2, false);
    g.addColour (0.35, juce::Colour (0xffaee6ff));
    g.addColour (0.52, juce::Colour (0xff3f8cff));
    g.addColour (0.60, juce::Colour (0xffeaf7ff));
    g.addColour (0.80, juce::Colour (0xff8fc4ff));
    return g;
}
} // namespace

Y2kLookAndFeel::Y2kLookAndFeel()
{
    refreshColours();
}

void Y2kLookAndFeel::refreshColours()
{
    setColour (juce::Label::textColourId, pal.textMain);
    setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xcc0a1428));
    setColour (juce::Slider::textBoxOutlineColourId, pal.accent1.withAlpha (0.55f));
    setColour (juce::ComboBox::textColourId, juce::Colour (0xffeaf7ff));
    setColour (juce::ComboBox::arrowColourId, pal.accent1.brighter (0.4f));
    setColour (juce::TextButton::textColourOffId, juce::Colour (0xffeaf7ff));
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::ToggleButton::textColourId, pal.textMain);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xf50a0f22));
    setColour (juce::PopupMenu::textColourId, juce::Colour (0xffeaf7ff));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, pal.accent1.darker (0.35f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colour (0xff031018));
}

juce::Font Y2kLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::Font (13.5f, juce::Font::bold);
}

juce::Font Y2kLookAndFeel::getPopupMenuFont()
{
    return juce::Font (13.5f);
}

void Y2kLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                       float sliderPos, float, float,
                                       juce::Slider::SliderStyle, juce::Slider& slider)
{
    const float trackY = (float) y + (float) height * 0.5f;
    auto track = juce::Rectangle<float> ((float) x, trackY - 3.0f, (float) width, 6.0f);
    g.setColour (juce::Colour (0xff0a1428));
    g.fillRoundedRectangle (track, 3.0f);
    g.setColour (pal.accent1.withAlpha (0.35f));
    g.drawRoundedRectangle (track, 3.0f, 1.0f);

    // Value portion: neon gradient with a soft glow underneath.
    auto value = track.withWidth (juce::jmax (6.0f, sliderPos - (float) x));
    g.setColour (pal.accent1.withAlpha (0.25f));
    g.fillRoundedRectangle (value.expanded (1.5f), 4.0f);
    g.setGradientFill (neonGradient (pal, (float) x, (float) x + (float) width, trackY));
    g.fillRoundedRectangle (value, 3.0f);

    // Chrome thumb with a neon ring.
    auto thumb = juce::Rectangle<float> (sliderPos - 8.0f, trackY - 8.0f, 16.0f, 16.0f);
    if (slider.isMouseOverOrDragging())
    {
        g.setColour (pal.accent1.withAlpha (0.4f));
        g.fillEllipse (thumb.expanded (4.0f));
    }
    juce::ColourGradient chrome (juce::Colour (0xfffdfdff), sliderPos - 4.0f, trackY - 5.0f,
                                 juce::Colour (0xff5f7fb8), sliderPos + 5.0f, trackY + 6.0f, true);
    g.setGradientFill (chrome);
    g.fillEllipse (thumb);
    g.setColour (pal.accent1);
    g.drawEllipse (thumb, 1.2f);
}

void Y2kLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                       float sliderPos, float rotaryStartAngle,
                                       float rotaryEndAngle, juce::Slider& slider)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 3.0f;
    auto centre = bounds.getCentre();
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // Value arc: glow pass + neon gradient.
    juce::Path arc;
    arc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                       rotaryStartAngle, angle, true);
    g.setColour (pal.accent1.withAlpha (0.3f));
    g.strokePath (arc, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));
    g.setGradientFill (neonGradient (pal, bounds.getX(), bounds.getRight(), centre.y));
    g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));
    // Track arc (full range, dim).
    juce::Path trackArc;
    trackArc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                            rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (juce::Colours::white.withAlpha (0.14f));
    g.strokePath (trackArc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

    // Chrome knob body.
    auto knob = juce::Rectangle<float> (centre.x - radius * 0.72f, centre.y - radius * 0.72f,
                                        radius * 1.44f, radius * 1.44f);
    if (slider.isMouseOverOrDragging())
    {
        g.setColour (pal.accent2.withAlpha (0.35f));
        g.fillEllipse (knob.expanded (4.0f));
    }
    juce::ColourGradient chrome (juce::Colour (0xfffdfdff), centre.x - radius * 0.3f,
                                 centre.y - radius * 0.35f,
                                 juce::Colour (0xff5f7fb8), centre.x + radius * 0.4f,
                                 centre.y + radius * 0.5f, true);
    g.setGradientFill (chrome);
    g.fillEllipse (knob);
    g.setColour (pal.accent1);
    g.drawEllipse (knob, 1.2f);

    // Pointer.
    juce::Path pointer;
    const float pointerLen = radius * 0.55f;
    pointer.startNewSubPath (centre.x, centre.y);
    pointer.lineTo (centre.x + pointerLen * std::sin (angle),
                    centre.y - pointerLen * std::cos (angle));
    g.setColour (pal.accent2);
    g.strokePath (pointer, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
}

void Y2kLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                   int buttonX, int buttonY, int buttonWidth, int buttonHeight,
                                   juce::ComboBox& box)
{
    auto b = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
    g.setColour (juce::Colour (0xdd0a1428));
    g.fillRoundedRectangle (b, 6.0f);
    g.setGradientFill (neonGradient (pal, 0.0f, (float) width, 0.0f));
    g.drawRoundedRectangle (b, 6.0f, 1.2f);

    const float cx = (float) buttonX + (float) buttonWidth * 0.5f;
    const float cy = (float) buttonY + (float) buttonHeight * 0.5f;
    juce::Path arrow;
    arrow.addTriangle (cx - 4.5f, cy - 2.5f, cx + 4.5f, cy - 2.5f, cx, cy + 3.5f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.fillPath (arrow);
}

void Y2kLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                           const juce::Colour&, bool highlighted, bool down)
{
    auto b = button.getLocalBounds().toFloat().reduced (0.5f);
    if (highlighted)
    {
        g.setColour (pal.accent2.withAlpha (0.19f));
        g.fillRoundedRectangle (b.expanded (2.0f), 8.0f);
    }
    g.setColour (down ? juce::Colour (0xff14405e) : juce::Colour (0xcc0a1428));
    g.fillRoundedRectangle (b, 6.0f);
    g.setGradientFill (neonGradient (pal, b.getX(), b.getRight(), b.getY()));
    g.setOpacity (highlighted || down ? 1.0f : 0.65f);
    g.drawRoundedRectangle (b, 6.0f, 1.2f);
    g.setOpacity (1.0f);
}

void Y2kLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                       bool highlighted, bool)
{
    auto bounds = button.getLocalBounds().toFloat();
    const float size = juce::jmin (18.0f, bounds.getHeight() - 6.0f);
    auto box = juce::Rectangle<float> (1.0f, bounds.getCentreY() - size * 0.5f, size, size);
    const bool ticked = button.getToggleState();

    g.setColour (juce::Colour (0xcc0a1428));
    g.fillRoundedRectangle (box, 4.0f);
    g.setGradientFill (neonGradient (pal, box.getX(), box.getRight(), box.getY()));
    g.setOpacity (highlighted || ticked ? 1.0f : 0.6f);
    g.drawRoundedRectangle (box, 4.0f, 1.2f);
    g.setOpacity (1.0f);

    if (ticked)
    {
        g.setGradientFill (neonGradient (pal, box.getX(), box.getRight(), box.getY()));
        g.fillRoundedRectangle (box.reduced (3.0f), 2.5f);
        auto c = box.getCentre();
        juce::Path tick;
        tick.startNewSubPath (c.x - 4.0f, c.y);
        tick.lineTo (c.x - 1.0f, c.y + 3.0f);
        tick.lineTo (c.x + 4.5f, c.y - 3.5f);
        g.setColour (juce::Colours::white);
        g.strokePath (tick, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    }

    g.setColour (button.findColour (juce::ToggleButton::textColourId, true));
    g.setFont (juce::Font (12.5f, juce::Font::bold));
    g.drawText (button.getButtonText(),
                juce::Rectangle<float> (box.getRight() + 5.0f, 0.0f,
                                        bounds.getWidth() - box.getRight() - 5.0f,
                                        bounds.getHeight()),
                juce::Justification::centredLeft);
}

//==============================================================================
// Editor

GridSliceXiphosEditor::GridSliceXiphosEditor (GridSliceXiphosProcessor& p)
    : AudioProcessorEditor (&p), proc (p), waveform (p)
{
    setLookAndFeel (&y2kLnf);
    setSize (980, 720);
    setResizable (true, true);
    setResizeLimits (780, 620, 1600, 1200);

    addAndMakeVisible (waveform);
    addAndMakeVisible (loadButton);
    addAndMakeVisible (exportButton);
    addAndMakeVisible (tutorialButton);
    addAndMakeVisible (fileLabel);
    addAndMakeVisible (infoLabel);
    fileLabel.setFont (juce::Font (14.0f, juce::Font::bold));

    loadButton.onClick = [this] { chooseAndLoadSample(); };
    exportButton.onClick = [this] { chooseAndExport(); };
    tutorialButton.onClick = [this] { showTutorial(); };
    reloadGuiImageIfNeeded();

    auto setupSlider = [this] (juce::Slider& s, juce::Label& l, const juce::String& text)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 20);
        addAndMakeVisible (s);
        l.setText (text, juce::dontSendNotification);
        l.setFont (juce::Font (12.5f, juce::Font::bold));
        addAndMakeVisible (l);
    };

    setupSlider (bpmSlider, bpmLabel, "BPM");
    setupSlider (offsetSlider, offsetLabel, "Offset ms");
    setupSlider (rootSlider, rootLabel, "Root Note");
    rootSlider.setRange (0, 84, 1);
    setupSlider (fadeSlider, fadeLabel, "Fade ms");
    setupSlider (cutoffSlider, cutoffLabel, "Cutoff Hz");
    setupSlider (resonanceSlider, resonanceLabel, "Reso Q");
    setupSlider (gainSlider, gainLabel, "Gain dB");
    setupSlider (imgOpacitySlider, imgOpacityLabel, "Img Opacity");

    auto setupCombo = [this] (juce::ComboBox& box, juce::Label& l, const juce::String& text,
                              juce::AudioParameterChoice& param)
    {
        box.addItemList (param.choices, 1);
        addAndMakeVisible (box);
        l.setText (text, juce::dontSendNotification);
        l.setFont (juce::Font (12.5f, juce::Font::bold));
        addAndMakeVisible (l);
    };

    auto& apvts = proc.getApvts();
    auto setupChoice = [this, &apvts, &setupCombo] (juce::ComboBox& box, juce::Label& l,
                                                    const juce::String& text, const char* pid)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (pid)))
            setupCombo (box, l, text, *p);
    };
    setupChoice (divisionBox, divisionLabel, "Division", GridSliceXiphosProcessor::pidDivision);
    setupChoice (playModeBox, playModeLabel, "Play Mode", GridSliceXiphosProcessor::pidPlayMode);
    setupChoice (filterBox, filterLabel, "Filter", GridSliceXiphosProcessor::pidFilterType);
    setupChoice (skinBox, skinLabel, "Skin", GridSliceXiphosProcessor::pidSkin);

    auto setupKnob = [this] (juce::Slider& s, juce::Label& l, const juce::String& text)
    {
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 58, 18);
        addAndMakeVisible (s);
        l.setText (text, juce::dontSendNotification);
        l.setFont (juce::Font (12.5f, juce::Font::bold));
        l.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (l);
    };
    setupKnob (attackSlider, attackLabel, "Attack");
    setupKnob (decaySlider, decayLabel, "Decay");
    setupKnob (sustainSlider, sustainLabel, "Sustain");
    setupKnob (releaseSlider, releaseLabel, "Release");
    setupChoice (uiColorBox, uiColorLabel, "Colour", GridSliceXiphosProcessor::pidUiColor);
    setupChoice (bgAspectBox, bgAspectLabel, "BG Aspect", GridSliceXiphosProcessor::pidBgAspect);
    setupChoice (sensitivityBox, sensitivityLabel, "Sensitivity", GridSliceXiphosProcessor::pidSensitivity);

    syncLabel.setText ("Sync", juce::dontSendNotification);
    syncLabel.setFont (juce::Font (12.5f, juce::Font::bold));
    addAndMakeVisible (syncLabel);
    addAndMakeVisible (syncToggle);

    bpmAtt.reset (new SliderAtt (apvts, GridSliceXiphosProcessor::pidBpm, bpmSlider));
    offsetAtt.reset (new SliderAtt (apvts, GridSliceXiphosProcessor::pidOffsetMs, offsetSlider));
    rootAtt.reset (new SliderAtt (apvts, GridSliceXiphosProcessor::pidRootNote, rootSlider));
    fadeAtt.reset (new SliderAtt (apvts, GridSliceXiphosProcessor::pidFadeMs, fadeSlider));
    cutoffAtt.reset (new SliderAtt (apvts, GridSliceXiphosProcessor::pidCutoff, cutoffSlider));
    resonanceAtt.reset (new SliderAtt (apvts, GridSliceXiphosProcessor::pidResonance, resonanceSlider));
    gainAtt.reset (new SliderAtt (apvts, GridSliceXiphosProcessor::pidGainDb, gainSlider));
    imgOpacityAtt.reset (new SliderAtt (apvts, GridSliceXiphosProcessor::pidImgOpacity, imgOpacitySlider));
    divisionAtt.reset (new ComboAtt (apvts, GridSliceXiphosProcessor::pidDivision, divisionBox));
    playModeAtt.reset (new ComboAtt (apvts, GridSliceXiphosProcessor::pidPlayMode, playModeBox));
    filterAtt.reset (new ComboAtt (apvts, GridSliceXiphosProcessor::pidFilterType, filterBox));
    skinAtt.reset (new ComboAtt (apvts, GridSliceXiphosProcessor::pidSkin, skinBox));
    skinBox.onChange = [this] { loadedImageKey.clear(); reloadGuiImageIfNeeded(); };
    syncAtt.reset (new ButtonAtt (apvts, GridSliceXiphosProcessor::pidSyncHost, syncToggle));

    attackAtt.reset (new SliderAtt (apvts, GridSliceXiphosProcessor::pidAttack, attackSlider));
    decayAtt.reset (new SliderAtt (apvts, GridSliceXiphosProcessor::pidDecay, decaySlider));
    sustainAtt.reset (new SliderAtt (apvts, GridSliceXiphosProcessor::pidSustain, sustainSlider));
    releaseAtt.reset (new SliderAtt (apvts, GridSliceXiphosProcessor::pidRelease, releaseSlider));
    uiColorAtt.reset (new ComboAtt (apvts, GridSliceXiphosProcessor::pidUiColor, uiColorBox));
    bgAspectAtt.reset (new ComboAtt (apvts, GridSliceXiphosProcessor::pidBgAspect, bgAspectBox));
    sensitivityAtt.reset (new ComboAtt (apvts, GridSliceXiphosProcessor::pidSensitivity, sensitivityBox));
    uiColorBox.onChange = [this] { applyPalette(); };
    bgAspectBox.onChange = [this] { repaint(); };
    sensitivityBox.onChange = [this] { applySensitivity(); };
    applyPalette();
    applySensitivity();

    waveform.onSliceSelected = [this] { updateInfoLabel(); };
    updateInfoLabel();
    startTimerHz (10);
}

GridSliceXiphosEditor::~GridSliceXiphosEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void GridSliceXiphosEditor::paint (juce::Graphics& g)
{
    const auto& pal = y2kLnf.palette();
    const auto bounds = getLocalBounds();
    g.fillAll (juce::Colour (0xff05070f));

    // Skin background image with aspect handling, dimmed by the Img
    // Opacity parameter so the controls stay readable.
    if (bgImage.isValid())
    {
        const int aspect = (int) proc.getApvts()
                                 .getRawParameterValue (GridSliceXiphosProcessor::pidBgAspect)
                                 ->load();
        const int iw = bgImage.getWidth(), ih = bgImage.getHeight();
        if (aspect == 0)
        {
            // Native: fit inside, preserving the image aspect (letterbox).
            const float scale = juce::jmin ((float) getWidth() / (float) iw,
                                            (float) getHeight() / (float) ih);
            const int dw = (int) (iw * scale), dh = (int) (ih * scale);
            g.drawImage (bgImage, (getWidth() - dw) / 2, (getHeight() - dh) / 2, dw, dh,
                         0, 0, iw, ih, false);
        }
        else if (aspect == 2)
        {
            // 16:10 -> 16:9: centre-crop the source to 16:9, then fill.
            const int srcH = juce::jmin (ih, (int) (iw * 9.0 / 16.0));
            g.drawImage (bgImage, 0, 0, getWidth(), getHeight(),
                         0, (ih - srcH) / 2, iw, srcH, false);
        }
        else
        {
            // 4:3 -> 16:9: stretch the whole image to fill.
            g.drawImage (bgImage, bounds.toFloat(),
                         juce::RectanglePlacement::fillDestination, false);
        }
        const float opacity = proc.getApvts()
                                  .getRawParameterValue (GridSliceXiphosProcessor::pidImgOpacity)
                                  ->load();
        g.setColour (juce::Colour (0xff04060f).withAlpha (1.0f - juce::jlimit (0.0f, 1.0f, opacity)));
        g.fillRect (bounds);
    }

    // Sparkle field (deterministic positions): tiny 4-point stars.
    {
        juce::Random rng (20261127);
        for (int i = 0; i < 46; ++i)
        {
            const float sx = rng.nextFloat() * (float) getWidth();
            const float sy = rng.nextFloat() * (float) getHeight();
            const float r = 2.0f + rng.nextFloat() * 4.0f;
            juce::Path star;
            star.startNewSubPath (sx, sy - r);
            star.lineTo (sx + r * 0.28f, sy - r * 0.28f);
            star.lineTo (sx + r, sy);
            star.lineTo (sx + r * 0.28f, sy + r * 0.28f);
            star.lineTo (sx, sy + r);
            star.lineTo (sx - r * 0.28f, sy + r * 0.28f);
            star.lineTo (sx - r, sy);
            star.lineTo (sx - r * 0.28f, sy - r * 0.28f);
            star.closeSubPath();
            g.setColour ((i % 3 == 0 ? pal.gold : pal.accent1)
                             .withAlpha (0.10f + rng.nextFloat() * 0.16f));
            g.fillPath (star);
        }
    }

    // Neon header divider with a soft glow.
    auto divider = juce::Rectangle<float> (0.0f, 35.0f, (float) getWidth(), 2.0f);
    g.setColour (pal.accent1.withAlpha (0.13f));
    g.fillRect (divider.expanded (0.0f, 3.0f));
    g.setGradientFill (neonGradient (pal, 0.0f, (float) getWidth(), 35.0f));
    g.fillRect (divider);

    // Hype strip: pachinko-style repeated catchphrases in gold.
    g.setColour (juce::Colour (0xff2a0518));
    g.fillRect (0, 38, getWidth(), 17);
    g.setColour (pal.gold);
    g.setFont (juce::Font (11.0f, juce::Font::bold));
    juce::String hype;
    for (int i = 0; i < 4; ++i)
        hype += juce::String::fromUTF8 ("★ BPM GRID SLICER ★ BMS READY ★ MIDI FILE NOT REQUIRED ★ DAW PLUGIN ★ ");
    g.drawText (hype, 6, 38, getWidth() * 2, 17, juce::Justification::left);

    // Chrome balloon title: neon glow passes under a chrome-gradient fill.
    const juce::Rectangle<int> titleArea (12, 4, 430, 28);
    g.setFont (juce::Font (20.0f, juce::Font::bold));
    g.setColour (pal.accent1.withAlpha (0.35f));
    g.drawText ("GridSliceXiphos - BPM Grid Slicer", titleArea.translated (-1, 0), juce::Justification::left);
    g.drawText ("GridSliceXiphos - BPM Grid Slicer", titleArea.translated (1, 0), juce::Justification::left);
    g.setColour (pal.accent2.withAlpha (0.25f));
    g.drawText ("GridSliceXiphos - BPM Grid Slicer", titleArea.translated (0, 1), juce::Justification::left);
    g.setGradientFill (chromeGradient (0.0f, 4.0f, 32.0f));
    g.drawText ("GridSliceXiphos - BPM Grid Slicer", titleArea, juce::Justification::left);

    // Beta badge: dark glass pill with a neon gradient rim.
    auto badge = juce::Rectangle<float> (452.0f, 8.0f, 64.0f, 20.0f);
    g.setColour (pal.accent2.withAlpha (0.19f));
    g.fillRoundedRectangle (badge.expanded (2.0f), 11.0f);
    g.setColour (juce::Colour (0xd91a0525));
    g.fillRoundedRectangle (badge, 10.0f);
    g.setGradientFill (neonGradient (pal, badge.getX(), badge.getRight(), badge.getY()));
    g.drawRoundedRectangle (badge, 10.0f, 1.2f);
    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (12.0f, juce::Font::bold));
    g.drawText ("Beta 2", badge.toNearestInt(), juce::Justification::centred);

    // Gold medal seals (top-right).
    {
        const char* medalText[3] = { "VST3", "BMS", "GRID" };
        juce::ColourGradient goldGrad (juce::Colour (0xfffff2b0), 0.0f, 0.0f,
                                       juce::Colour (0xffb8860b), 30.0f, 30.0f, false);
        for (int i = 0; i < 3; ++i)
        {
            auto c = juce::Rectangle<float> ((float) getWidth() - 172.0f + (float) i * 56.0f,
                                             4.0f, 30.0f, 30.0f);
            g.setGradientFill (goldGrad);
            g.fillEllipse (c);
            g.setColour (juce::Colour (0xff1a0f28));
            g.fillEllipse (c.reduced (3.0f));
            g.setColour (pal.gold);
            g.setFont (juce::Font (8.5f, juce::Font::bold));
            g.drawText (medalText[i], c.toNearestInt(), juce::Justification::centred);
        }
    }

    // Bottom feature checklist strip.
    g.setColour (juce::Colour (0xcc0a1428));
    g.fillRect (0, getHeight() - 20, getWidth(), 20);
    g.setColour (pal.gold.withAlpha (0.9f));
    g.setFont (juce::Font (11.0f, juce::Font::bold));
    g.drawText (juce::String::fromUTF8 ("✓ BPM指定で等間隔カット  ✓ MIDIファイル不要  ✓ 36進 #WAV 書き出し  ✓ ADSR搭載  ✓ スキン3種  ✓ 色覚対応パレット"),
                8, getHeight() - 20, getWidth() - 16, 20, juce::Justification::left);

    // Gold double frame with corner diamonds (maximalist trim).
    auto fb = bounds.toFloat().reduced (1.0f);
    juce::ColourGradient frameGrad (pal.gold, fb.getX(), fb.getY(),
                                    pal.gold.darker (0.5f), fb.getRight(), fb.getBottom(), false);
    g.setGradientFill (frameGrad);
    g.drawRect (fb, 2.0f);
    g.setColour (pal.gold.withAlpha (0.5f));
    g.drawRect (fb.reduced (5.0f), 1.0f);
    for (auto corner : { fb.getTopLeft(), fb.getTopRight(), fb.getBottomLeft(), fb.getBottomRight() })
    {
        const float r = 7.0f;
        juce::Path diamond;
        diamond.startNewSubPath (corner.x, corner.y - r);
        diamond.lineTo (corner.x + r, corner.y);
        diamond.lineTo (corner.x, corner.y + r);
        diamond.lineTo (corner.x - r, corner.y);
        diamond.closeSubPath();
        g.setGradientFill (frameGrad);
        g.fillPath (diamond);
    }
}

void GridSliceXiphosEditor::reloadGuiImageIfNeeded()
{
    const int skin = (int) proc.getApvts()
                             .getRawParameterValue (GridSliceXiphosProcessor::pidSkin)
                             ->load();
    const juce::String key = juce::String (skin);
    if (key == loadedImageKey)
        return;
    loadedImageKey = key;
    bgImage = juce::Image();

    auto loadEmbedded = [] (const char* data, int size)
    {
        juce::MemoryInputStream in (data, (size_t) size, false);
        return juce::ImageFileFormat::loadFrom (in);
    };

    switch (skin)
    {
        case 0: bgImage = loadEmbedded (BinaryData::skin_chrome_bloom_png, BinaryData::skin_chrome_bloom_pngSize); break;
        case 1: bgImage = loadEmbedded (BinaryData::skin_chrome_sky_png,   BinaryData::skin_chrome_sky_pngSize);   break;
        case 2: bgImage = loadEmbedded (BinaryData::skin_iridescent_png,   BinaryData::skin_iridescent_pngSize);   break;
        default: break; // 3 = None
    }
    repaint();
}

void GridSliceXiphosEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromBottom (20); // checklist strip (painted)
    area.removeFromTop (55);    // title + hype strip (painted)

    auto topRow = area.removeFromTop (30);
    loadButton.setBounds (topRow.removeFromLeft (120));
    topRow.removeFromLeft (8);
    exportButton.setBounds (topRow.removeFromLeft (120));
    topRow.removeFromLeft (8);
    tutorialButton.setBounds (topRow.removeFromLeft (96));
    topRow.removeFromLeft (12);
    fileLabel.setBounds (topRow);

    // Reserve the fixed-height control block at the bottom; the waveform
    // takes whatever space remains in the middle.
    auto bottomArea = area.removeFromBottom (34 * 4 + 6 * 4 + 16 + 88 + 8);
    area.removeFromTop (6);
    waveform.setBounds (area.removeFromTop (juce::jmax (140, area.getHeight() - 26)));
    area.removeFromTop (4);
    infoLabel.setBounds (area.removeFromTop (22));

    // Rows of controls inside bottomArea: each cell = label + control.
    auto placeRowIn = [] (juce::Rectangle<int>& src,
                          std::initializer_list<std::pair<juce::Component*, juce::Component*>> cells)
    {
        auto row = src.removeFromTop (34);
        const int cellW = row.getWidth() / (int) cells.size();
        for (auto& cell : cells)
        {
            auto c = row.removeFromLeft (cellW);
            cell.first->setBounds (c.removeFromLeft (58));
            c.removeFromLeft (2);
            cell.second->setBounds (c.reduced (0, 4));
        }
        src.removeFromTop (6);
    };
    placeRowIn (bottomArea, { { &bpmLabel, &bpmSlider }, { &syncLabel, &syncToggle },
                              { &divisionLabel, &divisionBox }, { &offsetLabel, &offsetSlider },
                              { &rootLabel, &rootSlider } });
    placeRowIn (bottomArea, { { &playModeLabel, &playModeBox }, { &fadeLabel, &fadeSlider },
                              { &filterLabel, &filterBox }, { &cutoffLabel, &cutoffSlider },
                              { &resonanceLabel, &resonanceSlider } });
    placeRowIn (bottomArea, { { &gainLabel, &gainSlider }, { &imgOpacityLabel, &imgOpacitySlider },
                              { &skinLabel, &skinBox } });
    placeRowIn (bottomArea, { { &uiColorLabel, &uiColorBox }, { &bgAspectLabel, &bgAspectBox },
                              { &sensitivityLabel, &sensitivityBox } });

    // ADSR knob row: labels strip + knobs.
    auto knobLabels = bottomArea.removeFromTop (16);
    auto knobRow = bottomArea.removeFromTop (88);
    juce::Label* kLabels[4] = { &attackLabel, &decayLabel, &sustainLabel, &releaseLabel };
    juce::Slider* kSliders[4] = { &attackSlider, &decaySlider, &sustainSlider, &releaseSlider };
    const int cellW = knobRow.getWidth() / 4;
    for (int i = 0; i < 4; ++i)
    {
        auto lc = knobLabels.removeFromLeft (cellW);
        kLabels[i]->setBounds (lc);
        auto kc = knobRow.removeFromLeft (cellW);
        kSliders[i]->setBounds (kc.withSizeKeepingCentre (juce::jmin (110, cellW - 8), 88));
    }
}

void GridSliceXiphosEditor::timerCallback()
{
    reloadGuiImageIfNeeded();
    updateInfoLabel();
    const int playing = proc.getCurrentlyPlayingSlice();
    if (playing != lastPlayingSlice)
    {
        lastPlayingSlice = playing;
        waveform.repaint();
    }
}

void GridSliceXiphosEditor::updateInfoLabel()
{
    auto snap = proc.currentSample();
    if (snap == nullptr)
    {
        fileLabel.setText ("(no sample)", juce::dontSendNotification);
        infoLabel.setText ("Load a sample to start slicing.", juce::dontSendNotification);
        return;
    }

    fileLabel.setText (snap->name + "  -  "
                       + juce::String (snap->buffer.getNumSamples() / juce::jmax (1.0, snap->sampleRate), 2)
                       + " sec / " + juce::String (snap->sampleRate, 0) + " Hz / "
                       + juce::String (snap->buffer.getNumChannels()) + " ch",
                       juce::dontSendNotification);

    const int count = proc.currentNumSlices();
    const double sliceLen = proc.currentSliceLengthSamples();
    const double sliceMs = sliceLen > 0 ? 1000.0 * sliceLen / snap->sampleRate : 0.0;
    const int root = (int) proc.getApvts().getRawParameterValue (GridSliceXiphosProcessor::pidRootNote)->load();

    juce::String text;
    text << "BPM " << juce::String (proc.effectiveBpm(), 1)
         << "  |  slices: " << count
         << "  |  slice length: " << juce::String (sliceMs, 2) << " ms ("
         << juce::String ((juce::int64) std::llround (sliceLen)) << " samples)"
         << "  |  MIDI notes " << root << " .. " << (root + juce::jmax (0, count - 1));
    if (waveform.selectedSlice >= 0)
        text << "  |  selected slice " << (waveform.selectedSlice + 1)
             << " (note " << (root + waveform.selectedSlice) << ")";
    infoLabel.setText (text, juce::dontSendNotification);
}

void GridSliceXiphosEditor::applyPalette()
{
    const int idx = (int) proc.getApvts()
                             .getRawParameterValue (GridSliceXiphosProcessor::pidUiColor)
                             ->load();
    if (idx == currentPaletteIndex)
        return;
    currentPaletteIndex = idx;
    const auto& pal = uiPalette (idx);
    y2kLnf.setPalette (pal);
    waveform.pal = pal;
    repaint();
    waveform.repaint();
}

void GridSliceXiphosEditor::applySensitivity()
{
    const int idx = (int) proc.getApvts()
                             .getRawParameterValue (GridSliceXiphosProcessor::pidSensitivity)
                             ->load();
    static const int dragPx[4] = { 420, 250, 140, 70 };
    const int drag = dragPx[juce::jlimit (0, 3, idx)];
    for (auto* s : { &bpmSlider, &offsetSlider, &rootSlider, &fadeSlider,
                     &cutoffSlider, &resonanceSlider, &gainSlider, &imgOpacitySlider,
                     &attackSlider, &decaySlider, &sustainSlider, &releaseSlider })
        s->setMouseDragSensitivity (drag);
}

void GridSliceXiphosEditor::showTutorial()
{
    static const char* tutorialText =
u8R"TXT(■ GridSliceXiphos とは
BPM と「何分音符か」を指定して、ファイル全体を等間隔に切り分けるスライサーです。
FL 内蔵の Slicex ではできない「BPM 指定の等間隔カット」がこのプラグインの核です。

【従来の音切りツールとの違い】
・MIDI ファイルを用意する必要がありません
・DAW 上のプラグインとしてそのまま動きます

■ 基本の使い方
1. 「Load Sample...」から音源を読み込む（波形へドラッグ＆ドロップでも可）
2. BPM と Division（分割の細かさ）を設定
3. スライスは Root Note から半音階で並びます。ピアノロールに階段状に置けば順番に再生できます
4. 波形をクリックするとそのスライスを試聴できます

■ Mid2BMS での音切りワークフロー
1. オートメーションやダッキングなどが入ったトラックを、まず WAV に書き出す
2. 書き出した WAV を GridSliceXiphos で開く
3. 分割する数字（Division）と BPM を設定する
4. 設定した分割数に応じて、FL 内蔵の Slicex のように基準音（Root Note）から
   ピアノロールへ階段状にノートを配置する
5. あとは Mid2BMS・bluemode で従来どおり音切りを行う
   （Export Slices... で 36 進 #WAV 形式のスライス書き出しもできます）

■ Grid ボーカルチョップ
ボーカル素材をグリッドで切って並べ替える使い方もできます。
Slicex の頭出し（先頭からの再生）とは違い、等間隔グリッド基準で
チョップできるのがポイントです。

■ その他の機能
・ADSR ノブ：Attack / Decay / Sustain / Release でスライスの鳴り方を調整
・Colour：色覚に合わせた UI パレット（Normal / P型 / D型 / T型）
・BG Aspect：壁紙の比率（Native / 4:3→16:9 / 16:10→16:9）
・Sensitivity：スライダーとノブのドラッグ感度

※ 本プラグインは Beta 版です。自己責任でお使いください。
)TXT";

    auto* text = new juce::TextEditor();
    text->setMultiLine (true);
    text->setReadOnly (true);
    text->setScrollbarsShown (true);
    text->setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff0a0f22));
    text->setColour (juce::TextEditor::textColourId, juce::Colour (0xffeaf7ff));
    text->setFont (juce::Font (14.0f));
    text->setText (juce::String::fromUTF8 (tutorialText));
    text->setCaretPosition (0);

    juce::DialogWindow::LaunchOptions opt;
    opt.dialogTitle = "GridSliceXiphos Tutorial";
    opt.dialogBackgroundColour = juce::Colour (0xff0a0f22);
    opt.content.setOwned (text);
    opt.content->setSize (580, 470);
    opt.resizable = true;
    opt.useBottomRightCornerResizer = true;
    opt.launchAsync();
}

void GridSliceXiphosEditor::chooseAndLoadSample()
{
    chooser = std::make_unique<juce::FileChooser> (
        "Load sample", juce::File(),
        proc.getFormatManager().getWildcardForAllFormats());
    chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (f.existsAsFile())
                              {
                                  proc.loadSample (f);
                                  waveform.selectedSlice = -1;
                                  waveform.repaint();
                                  updateInfoLabel();
                              }
                          });
}

void GridSliceXiphosEditor::chooseAndExport()
{
    if (proc.currentSample() == nullptr)
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::AlertWindow::WarningIcon, "GridSliceXiphos", "No sample loaded yet.");
        return;
    }
    chooser = std::make_unique<juce::FileChooser> (
        "Choose export folder", juce::File::getSpecialLocation (juce::File::userDesktopDirectory));
    chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectDirectories
                              | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto dir = fc.getResult();
                              if (dir == juce::File())
                                  return;
                              juce::String err;
                              const int n = proc.exportSlices (dir, err);
                              juce::AlertWindow::showMessageBoxAsync (
                                  juce::AlertWindow::InfoIcon, "GridSliceXiphos",
                                  n > 0 ? ("Exported " + juce::String (n) + " slices to:\n"
                                           + dir.getFullPathName()
                                           + "\n\n(+ bms/ subfolder and _map.txt)")
                                        : ("Export failed: " + err));
                          });
}
