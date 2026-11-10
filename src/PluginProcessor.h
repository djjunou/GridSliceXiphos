#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <memory>
#include "SliceGrid.h"

//==============================================================================
// GridSliceXiphos — a Slicex-style instrument whose slices are cut on a
// BPM x note-division grid (the thing Slicex cannot do).
//
//  - Load one sample (wav/aif/flac/mp3/ogg).
//  - Slices are generated from BPM + division + offset, from the head
//    of the file, at equal intervals. Nothing is detected or guessed.
//  - Slice i plays from MIDI note (rootNote + i), like Slicex.
//  - Optional host-tempo sync, per-plugin filter, slice WAV export
//    (including a BMS #WAV definition list).
//==============================================================================

struct LoadedSample
{
    juce::AudioBuffer<float> buffer;
    double sampleRate = 44100.0;
    juce::String name;
    juce::File file;
};

class GridSliceXiphosProcessor final : public juce::AudioProcessor
{
public:
    GridSliceXiphosProcessor();
    ~GridSliceXiphosProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "GridSliceXiphos"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    // Sample / grid API used by the editor (message thread).
    bool loadSample (const juce::File& file);
    std::shared_ptr<const LoadedSample> currentSample() const
    {
        return std::atomic_load_explicit (&sample, std::memory_order_acquire);
    }
    int getSampleVersion() const { return sampleVersion.load(); }

    double effectiveBpm() const;          // manual BPM or host BPM when synced
    double currentSliceLengthSamples() const;  // in file samples, 0 if no sample
    int    currentNumSlices() const;
    long long sliceStartSamples (int index) const;
    long long sliceEndSamples (int index) const;

    void auditionSlice (int index) { auditionRequest.store (index); }
    int  getCurrentlyPlayingSlice() const { return playingSlice.load(); }

    // Writes every slice as a WAV file plus "_map.txt" (and a bms/
    // subfolder with base-36 names + "#WAV" lines). Returns slice count.
    int exportSlices (const juce::File& outputDir, juce::String& errorMessage);

    juce::AudioProcessorValueTreeState& getApvts() { return apvts; }
    juce::AudioFormatManager& getFormatManager() { return formatManager; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Parameter IDs
    static constexpr const char* pidBpm        = "bpm";
    static constexpr const char* pidSyncHost   = "syncHost";
    static constexpr const char* pidDivision   = "division";
    static constexpr const char* pidOffsetMs   = "offsetMs";
    static constexpr const char* pidRootNote   = "rootNote";
    static constexpr const char* pidPlayMode   = "playMode";   // 0 one-shot, 1 gate
    static constexpr const char* pidFadeMs     = "fadeMs";
    static constexpr const char* pidFilterType = "filterType"; // 0 off 1 LP 2 HP 3 BP
    static constexpr const char* pidCutoff     = "cutoff";
    static constexpr const char* pidResonance  = "resonance";
    static constexpr const char* pidGainDb     = "gainDb";
    static constexpr const char* pidImgOpacity = "imgOpacity";
    static constexpr const char* pidSkin       = "skin"; // choice, see createParameterLayout

private:
    //==========================================================================
    struct Voice
    {
        bool active = false;
        bool releasing = false;
        int  sliceIndex = -1;
        int  midiNote = -1;
        double pos = 0.0;        // read position, in file samples
        double endPos = 0.0;     // slice end, in file samples
        float velocity = 1.0f;
        double env = 0.0;        // current envelope level 0..1
    };

    void startVoice (int sliceIndex, int midiNote, float velocity,
                     std::shared_ptr<const LoadedSample>& snap);
    void renderVoices (juce::AudioBuffer<float>& buffer,
                       std::shared_ptr<const LoadedSample>& snap);
    float paramFloat (const char* id) const
    {
        return apvts.getRawParameterValue (id)->load();
    }

    juce::AudioProcessorValueTreeState apvts;
    juce::AudioFormatManager formatManager;

    std::shared_ptr<const LoadedSample> sample; // accessed via atomic_load/store
    std::atomic<int> sampleVersion { 0 };
    std::atomic<int> auditionRequest { -1 };
    std::atomic<int> playingSlice { -1 };
    std::atomic<double> hostBpm { 0.0 };

    std::array<Voice, 32> voices;               // audio thread only
    juce::dsp::StateVariableTPTFilter<float> filterL, filterR;
    double currentHostSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GridSliceXiphosProcessor)
};
