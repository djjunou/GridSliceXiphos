#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

//==============================================================================
// UI colour palette. Index 0 is the original NEO Y2K neon scheme; 1-3 are
// colour-vision friendly variants (Protan / Deutan / Tritan) that avoid
// the hues each type cannot distinguish.
struct UiPalette
{
    juce::Colour accent1;   // primary neon (cyan family)
    juce::Colour accent2;   // secondary neon (magenta family)
    juce::Colour accent3;   // mid gradient stop (violet family)
    juce::Colour gold;      // maximalist gold accents
    juce::Colour textMain;  // label text
    juce::Colour waveCore;  // waveform core line
    juce::Colour gridLine;  // slice grid lines
};

const UiPalette& uiPalette (int index);

//==============================================================================
class WaveformView final : public juce::Component,
                           public juce::FileDragAndDropTarget
{
public:
    explicit WaveformView (GridSliceXiphosProcessor& p) : proc (p) {}

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

    int selectedSlice = -1;
    std::function<void()> onSliceSelected;
    UiPalette pal = uiPalette (0);

private:
    void rebuildPeaks();
    int cachedVersion = -1;
    int cachedWidth = 0;
    std::vector<float> peaksMin, peaksMax; // one pair per horizontal pixel

    GridSliceXiphosProcessor& proc;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformView)
};

//==============================================================================
// NEO Y2K / modern MONTAGEM-style look: deep-navy glass panels, neon cyan
// and magenta gradients, chrome highlights and soft glows, matching the
// bundled chrome-logo skin artworks.
class Y2kLookAndFeel : public juce::LookAndFeel_V4
{
public:
    Y2kLookAndFeel();

    void setPalette (const UiPalette& p) { pal = p; refreshColours(); }
    const UiPalette& palette() const { return pal; }

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonWidth, int buttonHeight,
                       juce::ComboBox&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;

private:
    void refreshColours();
    UiPalette pal = uiPalette (0);
};

//==============================================================================
class GridSliceXiphosEditor final : public juce::AudioProcessorEditor,
                               private juce::Timer
{
public:
    explicit GridSliceXiphosEditor (GridSliceXiphosProcessor& p);
    ~GridSliceXiphosEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void chooseAndLoadSample();
    void chooseAndExport();
    void reloadGuiImageIfNeeded();
    void updateInfoLabel();
    void showTutorial();
    void applyPalette();
    void applySensitivity();

    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    GridSliceXiphosProcessor& proc;
    Y2kLookAndFeel y2kLnf;

    WaveformView waveform;
    juce::TextButton loadButton { "Load Sample..." };
    juce::TextButton exportButton { "Export Slices..." };
    juce::TextButton tutorialButton { "Tutorial" };
    juce::Label fileLabel, infoLabel;

    juce::Image bgImage;            // current GUI background (skin) image
    juce::String loadedImageKey;    // skin index bgImage came from

    juce::Label bpmLabel, divisionLabel, offsetLabel, rootLabel,
                fadeLabel, cutoffLabel, resonanceLabel, gainLabel,
                playModeLabel, filterLabel, syncLabel, imgOpacityLabel, skinLabel;
    juce::Slider bpmSlider, offsetSlider, rootSlider, fadeSlider,
                 cutoffSlider, resonanceSlider, gainSlider, imgOpacitySlider;
    juce::ComboBox divisionBox, playModeBox, filterBox, skinBox;
    juce::ToggleButton syncToggle { "Host" };

    juce::Label attackLabel, decayLabel, sustainLabel, releaseLabel,
                uiColorLabel, bgAspectLabel, sensitivityLabel;
    juce::Slider attackSlider, decaySlider, sustainSlider, releaseSlider;
    juce::ComboBox uiColorBox, bgAspectBox, sensitivityBox;

    std::unique_ptr<SliderAtt> bpmAtt, offsetAtt, rootAtt, fadeAtt,
                               cutoffAtt, resonanceAtt, gainAtt, imgOpacityAtt;
    std::unique_ptr<ComboAtt> divisionAtt, playModeAtt, filterAtt, skinAtt;
    std::unique_ptr<ButtonAtt> syncAtt;
    std::unique_ptr<SliderAtt> attackAtt, decayAtt, sustainAtt, releaseAtt;
    std::unique_ptr<ComboAtt> uiColorAtt, bgAspectAtt, sensitivityAtt;
    int currentPaletteIndex = -1;

    std::unique_ptr<juce::FileChooser> chooser;
    int lastPlayingSlice = -2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GridSliceXiphosEditor)
};
