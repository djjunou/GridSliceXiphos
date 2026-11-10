#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
GridSliceXiphosProcessor::GridSliceXiphosProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    formatManager.registerBasicFormats();
}

GridSliceXiphosProcessor::~GridSliceXiphosProcessor() = default;

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout GridSliceXiphosProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pidBpm, 1 }, "BPM",
        NormalisableRange<float> (30.0f, 300.0f, 0.1f), 180.0f));

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { pidSyncHost, 1 }, "Sync to Host BPM", false));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { pidDivision, 1 }, "Division",
        StringArray { "1/1", "1/2", "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32", "1/64" },
        gridslice::divSixteenth));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pidOffsetMs, 1 }, "Offset (ms)",
        NormalisableRange<float> (0.0f, 2000.0f, 0.1f), 0.0f));

    layout.add (std::make_unique<AudioParameterInt> (
        ParameterID { pidRootNote, 1 }, "Root Note", 0, 84, 36));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { pidPlayMode, 1 }, "Play Mode",
        StringArray { "One-Shot", "Gate" }, 0));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pidFadeMs, 1 }, "Fade (ms)",
        NormalisableRange<float> (0.0f, 20.0f, 0.1f), 2.0f));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { pidFilterType, 1 }, "Filter",
        StringArray { "Off", "LowPass", "HighPass", "BandPass" }, 0));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pidCutoff, 1 }, "Cutoff (Hz)",
        NormalisableRange<float> (20.0f, 18000.0f, 1.0f, 0.3f), 18000.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pidResonance, 1 }, "Resonance (Q)",
        NormalisableRange<float> (0.3f, 8.0f, 0.01f), 0.7f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pidGainDb, 1 }, "Gain (dB)",
        NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pidImgOpacity, 1 }, "GUI Image Opacity",
        NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.35f));

    // Bundled skins are embedded in the binary (BinaryData). Index 4 is
    // the user's own PNG file (path stored in the plugin state), 5 = none.
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { pidSkin, 1 }, "Skin",
        StringArray { "Xiphos (Default)", "Bronze Blade", "Spartan Dawn",
                      "Marble Temple", "Custom PNG", "None" },
        0));

    return layout;
}

//==============================================================================
void GridSliceXiphosProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentHostSampleRate = sampleRate;
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 1 };
    filterL.prepare (spec);
    filterR.prepare (spec);
    filterL.reset();
    filterR.reset();
    for (auto& v : voices)
        v.active = false;
}

void GridSliceXiphosProcessor::releaseResources() {}

bool GridSliceXiphosProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
}

//==============================================================================
// Sample loading / grid maths (message thread, except where noted)

bool GridSliceXiphosProcessor::loadSample (const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (
        formatManager.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples <= 0)
        return false;

    auto loaded = std::make_shared<LoadedSample>();
    loaded->buffer.setSize ((int) reader->numChannels, (int) reader->lengthInSamples);
    reader->read (&loaded->buffer, 0, (int) reader->lengthInSamples, 0, true, true);
    loaded->sampleRate = reader->sampleRate;
    loaded->name = file.getFileName();
    loaded->file = file;

    std::atomic_store_explicit (&sample, std::shared_ptr<const LoadedSample> (loaded),
                                std::memory_order_release);
    sampleVersion.fetch_add (1);
    return true;
}

double GridSliceXiphosProcessor::effectiveBpm() const
{
    if (paramFloat (pidSyncHost) > 0.5f)
    {
        const double h = hostBpm.load();
        if (h > 20.0 && h < 400.0)
            return h;
    }
    return paramFloat (pidBpm);
}

double GridSliceXiphosProcessor::currentSliceLengthSamples() const
{
    auto snap = currentSample();
    if (snap == nullptr)
        return 0.0;
    return gridslice::samplesPerSlice (snap->sampleRate, effectiveBpm(),
                                       (int) paramFloat (pidDivision));
}

int GridSliceXiphosProcessor::currentNumSlices() const
{
    auto snap = currentSample();
    if (snap == nullptr)
        return 0;
    const double len = currentSliceLengthSamples();
    const long long offset = (long long) std::llround (
        paramFloat (pidOffsetMs) * 0.001 * snap->sampleRate);
    return gridslice::numSlices (snap->buffer.getNumSamples(), offset, len);
}

long long GridSliceXiphosProcessor::sliceStartSamples (int index) const
{
    auto snap = currentSample();
    if (snap == nullptr)
        return 0;
    const long long offset = (long long) std::llround (
        paramFloat (pidOffsetMs) * 0.001 * snap->sampleRate);
    return gridslice::sliceStart (index, offset, currentSliceLengthSamples());
}

long long GridSliceXiphosProcessor::sliceEndSamples (int index) const
{
    auto snap = currentSample();
    if (snap == nullptr)
        return 0;
    const long long offset = (long long) std::llround (
        paramFloat (pidOffsetMs) * 0.001 * snap->sampleRate);
    return gridslice::sliceEnd (index, offset, currentSliceLengthSamples(),
                                snap->buffer.getNumSamples());
}

//==============================================================================
// Voices (audio thread)

void GridSliceXiphosProcessor::startVoice (int sliceIndex, int midiNote, float velocity,
                                      std::shared_ptr<const LoadedSample>& snap)
{
    if (snap == nullptr || sliceIndex < 0 || sliceIndex >= currentNumSlices())
        return;

    Voice* target = nullptr;
    for (auto& v : voices)
        if (! v.active) { target = &v; break; }
    if (target == nullptr) // steal the first voice if all are busy
        target = &voices[0];

    target->active = true;
    target->releasing = false;
    target->sliceIndex = sliceIndex;
    target->midiNote = midiNote;
    target->pos = (double) sliceStartSamples (sliceIndex);
    target->endPos = (double) sliceEndSamples (sliceIndex);
    target->velocity = velocity;
    target->env = 0.0;
    playingSlice.store (sliceIndex);
}

void GridSliceXiphosProcessor::renderVoices (juce::AudioBuffer<float>& buffer,
                                        std::shared_ptr<const LoadedSample>& snap)
{
    const int numOut = buffer.getNumSamples();
    const int outChans = buffer.getNumChannels();
    if (snap == nullptr)
        return;

    const auto& src = snap->buffer;
    const int srcChans = src.getNumChannels();
    const double ratio = snap->sampleRate / currentHostSampleRate; // file samples per host sample
    const double fadeSamplesHost = juce::jmax (1.0, paramFloat (pidFadeMs) * 0.001 * currentHostSampleRate);
    const bool gateMode = paramFloat (pidPlayMode) > 0.5f;
    juce::ignoreUnused (gateMode); // note-off handling lives in processBlock

    bool anyActive = false;

    for (auto& v : voices)
    {
        if (! v.active)
            continue;
        anyActive = true;

        for (int i = 0; i < numOut; ++i)
        {
            if (! v.active)
                break;

            // Envelope: linear attack over fade, linear release at slice end / note-off.
            const double remainHost = (v.endPos - v.pos) / ratio;
            if (v.releasing || remainHost <= fadeSamplesHost)
                v.env = juce::jmax (0.0, v.env - 1.0 / fadeSamplesHost);
            else if (v.env < 1.0)
                v.env = juce::jmin (1.0, v.env + 1.0 / fadeSamplesHost);

            if ((v.releasing || remainHost <= 0.0) && v.env <= 0.0)
            {
                v.active = false;
                break;
            }
            if (v.pos >= v.endPos)
            {
                // Silent tail: let the fade-out finish without ever
                // reading past the slice end (no bleed into next slice).
                v.releasing = true;
                if (v.env <= 0.0)
                    v.active = false;
                continue;
            }

            // Linear-interpolated read, file samples -> stereo out.
            const long long idx = (long long) v.pos;
            const float frac = (float) (v.pos - (double) idx);
            const long long idx2 = std::min (idx + 1, (long long) src.getNumSamples() - 1);
            const float gain = (float) v.env * v.velocity;

            for (int ch = 0; ch < outChans; ++ch)
            {
                const int srcCh = juce::jmin (ch, srcChans - 1);
                const float s1 = src.getSample (srcCh, (int) idx);
                const float s2 = src.getSample (srcCh, (int) idx2);
                buffer.addSample (ch, i, (s1 + (s2 - s1) * frac) * gain);
            }
            v.pos += ratio;
        }
    }

    if (! anyActive)
        playingSlice.store (-1);
}

//==============================================================================
void GridSliceXiphosProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                        juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    auto snap = currentSample();

    // Host tempo (for optional sync + display)
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                hostBpm.store (*bpm);

    // Filter settings
    const int fType = (int) paramFloat (pidFilterType);
    if (fType > 0)
    {
        using FT = juce::dsp::StateVariableTPTFilterType;
        const auto type = fType == 1 ? FT::lowpass : (fType == 2 ? FT::highpass : FT::bandpass);
        for (auto* f : { &filterL, &filterR })
        {
            f->setType (type);
            f->setCutoffFrequency (paramFloat (pidCutoff));
            f->setResonance (paramFloat (pidResonance));
        }
    }

    const int rootNote = (int) paramFloat (pidRootNote);
    const bool gateMode = paramFloat (pidPlayMode) > 0.5f;

    // Audition request from the editor (waveform click)
    const int audition = auditionRequest.exchange (-1);
    if (audition >= 0 && snap != nullptr)
        startVoice (audition, rootNote + audition, 1.0f, snap);

    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn())
        {
            startVoice (msg.getNoteNumber() - rootNote, msg.getNoteNumber(),
                        msg.getFloatVelocity(), snap);
        }
        else if (msg.isNoteOff() && gateMode)
        {
            for (auto& v : voices)
                if (v.active && v.midiNote == msg.getNoteNumber())
                    v.releasing = true;
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            for (auto& v : voices)
                v.releasing = true;
        }
    }

    renderVoices (buffer, snap);

    if (fType > 0)
    {
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            buffer.setSample (0, i, filterL.processSample (0, buffer.getSample (0, i)));
            if (buffer.getNumChannels() > 1)
                buffer.setSample (1, i, filterR.processSample (0, buffer.getSample (1, i)));
        }
    }

    buffer.applyGain (juce::Decibels::decibelsToGain (paramFloat (pidGainDb)));
}

//==============================================================================
// Export (message thread)

int GridSliceXiphosProcessor::exportSlices (const juce::File& outputDir, juce::String& errorMessage)
{
    auto snap = currentSample();
    if (snap == nullptr)
    {
        errorMessage = "No sample loaded.";
        return 0;
    }

    const int count = currentNumSlices();
    if (count <= 0)
    {
        errorMessage = "No slices to export.";
        return 0;
    }

    if (! outputDir.createDirectory())
    {
        errorMessage = "Could not create output folder.";
        return 0;
    }
    auto bmsDir = outputDir.getChildFile ("bms");
    bmsDir.createDirectory();

    juce::WavAudioFormat wavFormat;
    const juce::String prefix = snap->file.getFileNameWithoutExtension();
    juce::String mapText;
    mapText << "# GridSliceXiphos export\n"
            << "# source: " << snap->name << "\n"
            << "# bpm: " << juce::String (effectiveBpm(), 2)
            << "  division: " << gridslice::divisionName ((int) paramFloat (pidDivision))
            << "  rootNote: " << (int) paramFloat (pidRootNote) << "\n\n";

    for (int i = 0; i < count; ++i)
    {
        const long long start = sliceStartSamples (i);
        const long long end = sliceEndSamples (i);
        const int len = (int) std::max<long long> (0, end - start);
        if (len <= 0)
            continue;

        juce::AudioBuffer<float> slice (snap->buffer.getNumChannels(), len);
        const int startInt = juce::jlimit (0, juce::jmax (0, snap->buffer.getNumSamples() - 1), (int) start);
        const int lenInt = juce::jlimit (0, snap->buffer.getNumSamples() - startInt, len);
        for (int ch = 0; ch < slice.getNumChannels(); ++ch)
            slice.copyFrom (ch, 0, snap->buffer, ch, startInt, lenInt);

        const juce::String decName = prefix + "_" + juce::String (i + 1).paddedLeft ('0', count >= 1000 ? 4 : 3) + ".wav";
        const juce::String bmsName = gridslice::bmsNumber (i) + ".wav";

        for (auto target : { outputDir.getChildFile (decName), bmsDir.getChildFile (bmsName) })
        {
            target.deleteFile();
            std::unique_ptr<juce::FileOutputStream> stream (target.createOutputStream());
            if (stream == nullptr)
                continue;
            std::unique_ptr<juce::AudioFormatWriter> writer (
                wavFormat.createWriterFor (stream.get(), snap->sampleRate,
                                           (unsigned int) slice.getNumChannels(),
                                           24, {}, 0));
            if (writer != nullptr)
            {
                writer->writeFromAudioSampleBuffer (slice, 0, len);
                stream.release(); // writer owns it now
            }
        }

        const double lenMs = 1000.0 * (double) len / snap->sampleRate;
        mapText << juce::String (i + 1).paddedLeft ('0', 3)
                << "  note " << ((int) paramFloat (pidRootNote) + i)
                << "  start " << (juce::int64) start
                << "  len " << juce::String (lenMs, 2) << " ms"
                << "  file " << decName << "\n";
    }

    mapText << "\n# BMS #WAV definitions (files in bms/ subfolder)\n";
    for (int i = 0; i < count; ++i)
        mapText << "#WAV" << gridslice::bmsNumber (i) << " "
                << gridslice::bmsNumber (i) << ".wav\n";

    outputDir.getChildFile ("_map.txt").replaceWithText (mapText);
    return count;
}

//==============================================================================
// State

void GridSliceXiphosProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto snap = currentSample())
        state.setProperty ("samplePath", snap->file.getFullPathName(), nullptr);
    state.setProperty ("guiImagePath", guiImagePath, nullptr);
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void GridSliceXiphosProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml == nullptr)
        return;
    auto tree = juce::ValueTree::fromXml (*xml);
    if (! tree.isValid())
        return;
    apvts.replaceState (tree);
    guiImagePath = tree.getProperty ("guiImagePath", "").toString();
    const auto path = tree.getProperty ("samplePath", "").toString();
    if (path.isNotEmpty())
    {
        juce::File f (path);
        if (f.existsAsFile())
            loadSample (f);
    }
}

//==============================================================================
juce::AudioProcessorEditor* GridSliceXiphosProcessor::createEditor()
{
    return new GridSliceXiphosEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GridSliceXiphosProcessor();
}
