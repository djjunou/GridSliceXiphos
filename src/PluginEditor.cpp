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
    g.fillAll (juce::Colour (0xe614161c)); // slightly translucent: GUI image shows through

    auto snap = proc.currentSample();
    if (snap == nullptr)
    {
        g.setColour (juce::Colours::grey);
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
        g.setColour (juce::Colour (0x5540c8ff));
        g.fillRect (sliceRect (playing));
    }
    if (selectedSlice >= 0 && selectedSlice < count && selectedSlice != playing)
    {
        g.setColour (juce::Colour (0x33ffffff));
        g.fillRect (sliceRect (selectedSlice));
    }

    // Waveform
    g.setColour (juce::Colour (0xff7fd4ff));
    for (int x = 0; x < getWidth() && x < (int) peaksMin.size(); ++x)
        g.drawVerticalLine (x, mid + peaksMin[(size_t) x] * amp, mid + peaksMax[(size_t) x] * amp);

    // Slice grid: bar lines (every slice that starts a beat group) stronger.
    g.setFont (juce::Font (10.0f));
    for (int i = 0; i < count; ++i)
    {
        const float x = (float) proc.sliceStartSamples (i) / (float) total * bounds.getWidth();
        g.setColour (juce::Colour (0xaaFFB000));
        g.drawVerticalLine ((int) x, 0.0f, bounds.getHeight());

        const float sliceW = bounds.getWidth() / (float) juce::jmax (1, count);
        if (sliceW > 16.0f || i % 4 == 0)
        {
            g.setColour (juce::Colours::white.withAlpha (0.75f));
            g.drawText (juce::String (i + 1), (int) x + 2, 2, 30, 12,
                        juce::Justification::left);
        }
    }
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
// Editor

GridSliceXiphosEditor::GridSliceXiphosEditor (GridSliceXiphosProcessor& p)
    : AudioProcessorEditor (&p), proc (p), waveform (p)
{
    setSize (920, 660);
    setResizable (true, true);
    setResizeLimits (720, 540, 1600, 1100);

    addAndMakeVisible (waveform);
    addAndMakeVisible (loadButton);
    addAndMakeVisible (exportButton);
    addAndMakeVisible (setImageButton);
    addAndMakeVisible (clearImageButton);
    addAndMakeVisible (fileLabel);
    addAndMakeVisible (infoLabel);
    fileLabel.setFont (juce::Font (14.0f, juce::Font::bold));

    loadButton.onClick = [this] { chooseAndLoadSample(); };
    exportButton.onClick = [this] { chooseAndExport(); };
    setImageButton.onClick = [this] { chooseGuiImage(); };
    clearImageButton.onClick = [this]
    {
        // Switch the skin selector to "None" (index 5).
        if (auto* p = proc.getApvts().getParameter (GridSliceXiphosProcessor::pidSkin))
            p->setValueNotifyingHost (p->convertTo0to1 (5.0f));
        proc.setGuiImagePath ({});
        loadedImageKey.clear();
        reloadGuiImageIfNeeded();
    };
    reloadGuiImageIfNeeded();

    auto setupSlider = [this] (juce::Slider& s, juce::Label& l, const juce::String& text)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 20);
        addAndMakeVisible (s);
        l.setText (text, juce::dontSendNotification);
        l.setFont (juce::Font (12.0f));
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
        l.setFont (juce::Font (12.0f));
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
    syncLabel.setFont (juce::Font (12.0f));
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
}

void GridSliceXiphosEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e2128));

    // User-chosen background image (PNG/JPG), dimmed by the Img Opacity
    // parameter so controls stay readable.
    if (bgImage.isValid())
    {
        g.drawImage (bgImage, getLocalBounds().toFloat(),
                     juce::RectanglePlacement::fillDestination, false);
        const float opacity = proc.getApvts()
                                  .getRawParameterValue (GridSliceXiphosProcessor::pidImgOpacity)
                                  ->load();
        g.setColour (juce::Colour (0xff1e2128).withAlpha (1.0f - juce::jlimit (0.0f, 1.0f, opacity)));
        g.fillRect (getLocalBounds());
    }

    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (18.0f, juce::Font::bold));
    g.drawText ("GridSliceXiphos — BPM Grid Slicer", 12, 6, 420, 26, juce::Justification::left);
}

void GridSliceXiphosEditor::reloadGuiImageIfNeeded()
{
    const int skin = (int) proc.getApvts()
                             .getRawParameterValue (GridSliceXiphosProcessor::pidSkin)
                             ->load();
    const auto customPath = proc.getGuiImagePath();
    const juce::String key = juce::String (skin) + "|" + (skin == 4 ? customPath : juce::String());
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
        case 0: bgImage = loadEmbedded (BinaryData::skin_xiphos_png,  BinaryData::skin_xiphos_pngSize);  break;
        case 1: bgImage = loadEmbedded (BinaryData::skin_bronze_png,  BinaryData::skin_bronze_pngSize);  break;
        case 2: bgImage = loadEmbedded (BinaryData::skin_spartan_png, BinaryData::skin_spartan_pngSize); break;
        case 3: bgImage = loadEmbedded (BinaryData::skin_marble_png,  BinaryData::skin_marble_pngSize);  break;
        case 4:
            if (customPath.isNotEmpty())
            {
                juce::File f (customPath);
                if (f.existsAsFile())
                    bgImage = juce::ImageFileFormat::loadFrom (f);
            }
            break;
        default: break; // 5 = None
    }
    repaint();
}

void GridSliceXiphosEditor::chooseGuiImage()
{
    chooser = std::make_unique<juce::FileChooser> (
        "Choose a GUI background image (PNG)",
        juce::File::getSpecialLocation (juce::File::userPicturesDirectory),
        "*.png;*.jpg;*.jpeg");
    chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (f.existsAsFile())
                              {
                                  proc.setGuiImagePath (f.getFullPathName());
                                  // Switch the skin selector to "Custom PNG" (index 4).
                                  if (auto* p = proc.getApvts().getParameter (
                                          GridSliceXiphosProcessor::pidSkin))
                                      p->setValueNotifyingHost (p->convertTo0to1 (4.0f));
                                  loadedImageKey.clear();
                                  reloadGuiImageIfNeeded();
                              }
                          });
}

void GridSliceXiphosEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (38); // title

    auto topRow = area.removeFromTop (30);
    loadButton.setBounds (topRow.removeFromLeft (120));
    topRow.removeFromLeft (6);
    exportButton.setBounds (topRow.removeFromLeft (120));
    topRow.removeFromLeft (6);
    setImageButton.setBounds (topRow.removeFromLeft (130));
    topRow.removeFromLeft (6);
    clearImageButton.setBounds (topRow.removeFromLeft (90));
    topRow.removeFromLeft (10);
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

    fileLabel.setText (snap->name + "  —  "
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
