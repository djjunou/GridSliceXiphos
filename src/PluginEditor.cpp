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
        g.setColour (juce::Colour (0xff8fd8ff));
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
        g.setColour (juce::Colour (0x554de8ff));
        g.fillRect (sliceRect (playing));
    }
    if (selectedSlice >= 0 && selectedSlice < count && selectedSlice != playing)
    {
        g.setColour (juce::Colour (0x40ff4dd8));
        g.fillRect (sliceRect (selectedSlice));
    }

    // Waveform: cyan glow pass under a bright core.
    g.setColour (juce::Colour (0x594de8ff));
    for (int x = 0; x < getWidth() && x < (int) peaksMin.size(); ++x)
        g.drawVerticalLine (x, mid + peaksMin[(size_t) x] * amp - 1.0f,
                            mid + peaksMax[(size_t) x] * amp + 1.0f);
    g.setColour (juce::Colour (0xff8df5ff));
    for (int x = 0; x < getWidth() && x < (int) peaksMin.size(); ++x)
        g.drawVerticalLine (x, mid + peaksMin[(size_t) x] * amp, mid + peaksMax[(size_t) x] * amp);

    // Slice grid: bar lines (every slice that starts a beat group) stronger.
    g.setFont (juce::Font (10.0f));
    for (int i = 0; i < count; ++i)
    {
        const float x = (float) proc.sliceStartSamples (i) / (float) total * bounds.getWidth();
        g.setColour (juce::Colour (0xccff4dd8));
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
    g.setColour (juce::Colour (0x664de8ff));
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

namespace
{
juce::ColourGradient neonGradient (float x1, float x2, float y)
{
    juce::ColourGradient g (juce::Colour (0xff4de8ff), x1, y,
                            juce::Colour (0xffff4dd8), x2, y, false);
    g.addColour (0.5, juce::Colour (0xff7a5cff));
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
    setColour (juce::Label::textColourId, juce::Colour (0xffd9f2ff));
    setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xcc0a1428));
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0x884de8ff));
    setColour (juce::ComboBox::textColourId, juce::Colour (0xffeaf7ff));
    setColour (juce::ComboBox::arrowColourId, juce::Colour (0xff7df3ff));
    setColour (juce::TextButton::textColourOffId, juce::Colour (0xffeaf7ff));
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::ToggleButton::textColourId, juce::Colour (0xffd9f2ff));
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xf50a0f22));
    setColour (juce::PopupMenu::textColourId, juce::Colour (0xffeaf7ff));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff1fb9d8));
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
    g.setColour (juce::Colour (0x554de8ff));
    g.drawRoundedRectangle (track, 3.0f, 1.0f);

    // Value portion: neon gradient with a soft glow underneath.
    auto value = track.withWidth (juce::jmax (6.0f, sliderPos - (float) x));
    g.setColour (juce::Colour (0x404de8ff));
    g.fillRoundedRectangle (value.expanded (1.5f), 4.0f);
    g.setGradientFill (neonGradient ((float) x, (float) x + (float) width, trackY));
    g.fillRoundedRectangle (value, 3.0f);

    // Chrome thumb with cyan ring.
    auto thumb = juce::Rectangle<float> (sliderPos - 8.0f, trackY - 8.0f, 16.0f, 16.0f);
    if (slider.isMouseOverOrDragging())
    {
        g.setColour (juce::Colour (0x664de8ff));
        g.fillEllipse (thumb.expanded (4.0f));
    }
    juce::ColourGradient chrome (juce::Colour (0xfffdfdff), sliderPos - 4.0f, trackY - 5.0f,
                                 juce::Colour (0xff5f7fb8), sliderPos + 5.0f, trackY + 6.0f, true);
    g.setGradientFill (chrome);
    g.fillEllipse (thumb);
    g.setColour (juce::Colour (0xff4de8ff));
    g.drawEllipse (thumb, 1.2f);
}

void Y2kLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                   int buttonX, int buttonY, int buttonWidth, int buttonHeight,
                                   juce::ComboBox& box)
{
    auto b = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
    g.setColour (juce::Colour (0xdd0a1428));
    g.fillRoundedRectangle (b, 6.0f);
    g.setGradientFill (neonGradient (0.0f, (float) width, 0.0f));
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
        g.setColour (juce::Colour (0x30ff4dd8));
        g.fillRoundedRectangle (b.expanded (2.0f), 8.0f);
    }
    g.setColour (down ? juce::Colour (0xff14405e) : juce::Colour (0xcc0a1428));
    g.fillRoundedRectangle (b, 6.0f);
    g.setGradientFill (neonGradient (b.getX(), b.getRight(), b.getY()));
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
    g.setGradientFill (neonGradient (box.getX(), box.getRight(), box.getY()));
    g.setOpacity (highlighted || ticked ? 1.0f : 0.6f);
    g.drawRoundedRectangle (box, 4.0f, 1.2f);
    g.setOpacity (1.0f);

    if (ticked)
    {
        g.setGradientFill (neonGradient (box.getX(), box.getRight(), box.getY()));
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
    setSize (920, 660);
    setResizable (true, true);
    setResizeLimits (720, 540, 1600, 1100);

    addAndMakeVisible (waveform);
    addAndMakeVisible (loadButton);
    addAndMakeVisible (exportButton);
    addAndMakeVisible (fileLabel);
    addAndMakeVisible (infoLabel);
    fileLabel.setFont (juce::Font (14.0f, juce::Font::bold));

    loadButton.onClick = [this] { chooseAndLoadSample(); };
    exportButton.onClick = [this] { chooseAndExport(); };
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
    g.fillAll (juce::Colour (0xff05070f));

    // Skin background image, dimmed by the Img Opacity parameter so the
    // controls stay readable.
    if (bgImage.isValid())
    {
        g.drawImage (bgImage, getLocalBounds().toFloat(),
                     juce::RectanglePlacement::fillDestination, false);
        const float opacity = proc.getApvts()
                                  .getRawParameterValue (GridSliceXiphosProcessor::pidImgOpacity)
                                  ->load();
        g.setColour (juce::Colour (0xff04060f).withAlpha (1.0f - juce::jlimit (0.0f, 1.0f, opacity)));
        g.fillRect (getLocalBounds());
    }

    // Neon header divider (cyan -> violet -> magenta) with a soft glow.
    auto divider = juce::Rectangle<float> (0.0f, 35.0f, (float) getWidth(), 2.0f);
    g.setColour (juce::Colour (0x224de8ff));
    g.fillRect (divider.expanded (0.0f, 3.0f));
    g.setGradientFill (neonGradient (0.0f, (float) getWidth(), 35.0f));
    g.fillRect (divider);

    // Chrome balloon title: neon glow passes under a chrome-gradient fill.
    const juce::Rectangle<int> titleArea (12, 4, 430, 28);
    g.setFont (juce::Font (20.0f, juce::Font::bold));
    g.setColour (juce::Colour (0x594de8ff));
    g.drawText ("GridSliceXiphos - BPM Grid Slicer", titleArea.translated (-1, 0), juce::Justification::left);
    g.drawText ("GridSliceXiphos - BPM Grid Slicer", titleArea.translated (1, 0), juce::Justification::left);
    g.setColour (juce::Colour (0x40ff4dd8));
    g.drawText ("GridSliceXiphos - BPM Grid Slicer", titleArea.translated (0, 1), juce::Justification::left);
    g.setGradientFill (chromeGradient (0.0f, 4.0f, 32.0f));
    g.drawText ("GridSliceXiphos - BPM Grid Slicer", titleArea, juce::Justification::left);

    // Beta badge: dark glass pill with a neon gradient rim.
    auto badge = juce::Rectangle<float> (452.0f, 8.0f, 64.0f, 20.0f);
    g.setColour (juce::Colour (0x30ff4dd8));
    g.fillRoundedRectangle (badge.expanded (2.0f), 11.0f);
    g.setColour (juce::Colour (0xd91a0525));
    g.fillRoundedRectangle (badge, 10.0f);
    g.setGradientFill (neonGradient (badge.getX(), badge.getRight(), badge.getY()));
    g.drawRoundedRectangle (badge, 10.0f, 1.2f);
    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (12.0f, juce::Font::bold));
    g.drawText ("Beta 1", badge.toNearestInt(), juce::Justification::centred);
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
    area.removeFromTop (38); // title

    auto topRow = area.removeFromTop (30);
    loadButton.setBounds (topRow.removeFromLeft (130));
    topRow.removeFromLeft (8);
    exportButton.setBounds (topRow.removeFromLeft (130));
    topRow.removeFromLeft (12);
    fileLabel.setBounds (topRow);

    area.removeFromTop (6);
    waveform.setBounds (area.removeFromTop (230));
    area.removeFromTop (4);
    infoLabel.setBounds (area.removeFromTop (22));
    area.removeFromTop (8);

    // Two rows of controls: each cell = label (34 wide) + control
    auto placeRow = [&area] (std::initializer_list<std::pair<juce::Component*, juce::Component*>> cells)
    {
        auto row = area.removeFromTop (34);
        const int cellW = row.getWidth() / (int) cells.size();
        for (auto& cell : cells)
        {
            auto c = row.removeFromLeft (cellW);
            cell.first->setBounds (c.removeFromLeft (58));
            c.removeFromLeft (2);
            cell.second->setBounds (c.reduced (0, 4));
        }
        area.removeFromTop (6);
    };

    placeRow ({ { &bpmLabel, &bpmSlider }, { &syncLabel, &syncToggle },
                { &divisionLabel, &divisionBox }, { &offsetLabel, &offsetSlider },
                { &rootLabel, &rootSlider } });
    placeRow ({ { &playModeLabel, &playModeBox }, { &fadeLabel, &fadeSlider },
                { &filterLabel, &filterBox }, { &cutoffLabel, &cutoffSlider },
                { &resonanceLabel, &resonanceSlider } });
    placeRow ({ { &gainLabel, &gainSlider }, { &imgOpacityLabel, &imgOpacitySlider },
                { &skinLabel, &skinBox } });
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
