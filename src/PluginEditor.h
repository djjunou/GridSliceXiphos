#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

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

private:
    void rebuildPeaks();
    int cachedVersion = -1;
    int cachedWidth = 0;
    std::vector<float> peaksMin, peaksMax; // one pair per horizontal pixel

    GridSliceXiphosProcessor& proc;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformView)
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
    void chooseGuiImage();
    void reloadGuiImageIfNeeded();
    void updateInfoLabel();

    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    GridSliceXiphosProcessor& proc;

    WaveformView waveform;
    juce::TextButton loadButton { "Load Sample..." };
    juce::TextButton exportButton { "Export Slices..." };
    juce::TextButton setImageButton { "Set PNG Image..." };
    juce::TextButton clearImageButton { "Clear Image" };
    juce::Label fileLabel, infoLabel;

    juce::Image bgImage;            // current GUI background (skin) image
    juce::String loadedImageKey;    // "skinIndex|customPath" bgImage came from

    juce::Label bpmLabel, divisionLabel, offsetLabel, rootLabel,
                fadeLabel, cutoffLabel, resonanceLabel, gainLabel,
                playModeLabel, filterLabel, syncLabel, imgOpacityLabel, skinLabel;
    juce::Slider bpmSlider, offsetSlider, rootSlider, fadeSlider,
                 cutoffSlider, resonanceSlider, gainSlider, imgOpacitySlider;
    juce::ComboBox divisionBox, playModeBox, filterBox, skinBox;
    juce::ToggleButton syncToggle { "Host" };

    std::unique_ptr<SliderAtt> bpmAtt, offsetAtt, rootAtt, fadeAtt,
                               cutoffAtt, resonanceAtt, gainAtt, imgOpacityAtt;
    std::unique_ptr<ComboAtt> divisionAtt, playModeAtt, filterAtt, skinAtt;
    std::unique_ptr<ButtonAtt> syncAtt;

    std::unique_ptr<juce::FileChooser> chooser;
    int lastPlayingSlice = -2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GridSliceXiphosEditor)
};
