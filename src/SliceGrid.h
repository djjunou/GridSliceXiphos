#pragma once

// SliceGrid.h — pure BPM-grid slice mathematics for GridSliceXiphos.
// No JUCE dependency: this header is shared by the plugin and the
// standalone unit test (tests/test_slicegrid.cpp), and mirrors the
// math in the companion CLI tool (tools/gridslice.py).
//
// Core idea (the feature Slicex does not have):
//   Given a tempo (BPM) and a note division, slice a sample into
//   EQUAL intervals starting from the head of the file (+ offset).
//
//   samplesPerSlice = sampleRate * 60 / BPM * beatsPerDivision
//   e.g. 44.1 kHz, BPM 180, 1/16 note -> 44100 * 60/180 * 0.25 = 3675 samples.

#include <algorithm>
#include <cmath>
#include <string>

namespace gridslice
{

// Division indices — must match the AudioParameterChoice order in
// PluginProcessor::createParameterLayout().
enum Division
{
    divWhole        = 0, // 1/1
    divHalf         = 1, // 1/2
    divQuarter      = 2, // 1/4
    divEighth       = 3, // 1/8
    divEighthT      = 4, // 1/8 triplet
    divSixteenth    = 5, // 1/16
    divSixteenthT   = 6, // 1/16 triplet
    divThirtySecond = 7, // 1/32
    divSixtyFourth  = 8, // 1/64
    numDivisions    = 9
};

inline double beatsForDivision (int divisionIndex)
{
    switch (divisionIndex)
    {
        case divWhole:        return 4.0;
        case divHalf:         return 2.0;
        case divQuarter:      return 1.0;
        case divEighth:       return 0.5;
        case divEighthT:      return 1.0 / 3.0;
        case divSixteenth:    return 0.25;
        case divSixteenthT:   return 1.0 / 6.0;
        case divThirtySecond: return 0.125;
        case divSixtyFourth:  return 0.0625;
        default:              return 0.25;
    }
}

inline const char* divisionName (int divisionIndex)
{
    static const char* names[numDivisions] = {
        "1/1", "1/2", "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32", "1/64"
    };
    if (divisionIndex < 0 || divisionIndex >= numDivisions)
        return "?";
    return names[divisionIndex];
}

// Length of one slice, in samples of the *file's* sample rate.
inline double samplesPerSlice (double fileSampleRate, double bpm, int divisionIndex)
{
    if (fileSampleRate <= 0.0 || bpm <= 0.0)
        return 0.0;
    return fileSampleRate * 60.0 / bpm * beatsForDivision (divisionIndex);
}

// Number of slices needed to cover [offset, totalSamples).
inline int numSlices (long long totalSamples, long long offsetSamples, double sliceLength)
{
    if (sliceLength <= 0.0 || totalSamples <= offsetSamples)
        return 0;
    const double usable = (double) (totalSamples - offsetSamples);
    // Subtract a tiny epsilon so an exactly-dividing file does not
    // produce a phantom empty slice at the end.
    return (int) std::ceil (usable / sliceLength - 1.0e-9);
}

// Start sample of slice i (0-based). Boundaries are rounded from the
// ideal fractional position each time, so rounding error never
// accumulates across slices.
inline long long sliceStart (int index, long long offsetSamples, double sliceLength)
{
    return offsetSamples + (long long) std::llround (index * sliceLength);
}

// End sample (exclusive) of slice i, clamped to the file length.
inline long long sliceEnd (int index, long long offsetSamples, double sliceLength,
                           long long totalSamples)
{
    return std::min (totalSamples, sliceStart (index + 1, offsetSamples, sliceLength));
}

// BMS #WAV numbers are two base-36 digits (01..0Z, 10..ZZ).
// Slice 0 (0-based) maps to #WAV01, slice 9 -> #WAV0A, slice 35 -> #WAV10.
inline std::string bmsNumber (int sliceIndexZeroBased)
{
    static const char* digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    int n = sliceIndexZeroBased + 1;
    if (n < 0)   n = 0;
    if (n > 1295) n = 1295; // ZZ
    std::string s;
    s += digits[n / 36];
    s += digits[n % 36];
    return s;
}

} // namespace gridslice
