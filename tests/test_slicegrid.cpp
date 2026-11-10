// Unit tests for the BPM-grid slice math (SliceGrid.h).
// The headline case is the user's example: BPM 180, 16th notes, 44.1 kHz
// -> exactly 3675 samples per slice.
#include "SliceGrid.h"
#include <cmath>
#include <cstdio>

static int failures = 0;

#define CHECK(cond) do { if (! (cond)) { \
    std::printf ("FAIL line %d: %s\n", __LINE__, #cond); ++failures; } } while (0)
#define CHECK_NEAR(a, b) do { double _a = (a), _b = (b); if (std::abs (_a - _b) > 1e-6) { \
    std::printf ("FAIL line %d: %s = %f, expected %f\n", __LINE__, #a, _a, _b); ++failures; } } while (0)

int main()
{
    using namespace gridslice;

    // BPM 180, 1/16, 44.1k -> 3675 samples exactly
    CHECK_NEAR (samplesPerSlice (44100.0, 180.0, divSixteenth), 3675.0);
    // 1 second of audio -> 12 slices, last one ends at the file end
    CHECK (numSlices (44100, 0, 3675.0) == 12);
    CHECK (sliceStart (11, 0, 3675.0) == 40425);
    CHECK (sliceEnd (11, 0, 3675.0, 44100) == 44100);

    // BPM 120, 1/4, 44.1k -> 22050 samples
    CHECK_NEAR (samplesPerSlice (44100.0, 120.0, divQuarter), 22050.0);

    // Triplet: BPM 180, 1/8T, 44.1k -> 4900 samples (matches CLI tool result)
    CHECK_NEAR (samplesPerSlice (44100.0, 180.0, divEighthT), 4900.0);
    CHECK (numSlices (44100, 0, 4900.0) == 9);

    // 48k, BPM 120, 1/16 -> 6000 samples
    CHECK_NEAR (samplesPerSlice (48000.0, 120.0, divSixteenth), 6000.0);

    // Offset: 1000 samples in, slicing starts there
    CHECK (sliceStart (0, 1000, 3675.0) == 1000);
    CHECK (numSlices (44100, 1000, 3675.0) == 12); // ceil(43100/3675)=12

    // No phantom slice when the file divides exactly
    CHECK (numSlices (44100, 0, 22050.0) == 2);
    // Empty / degenerate inputs
    CHECK (numSlices (100, 100, 3675.0) == 0);
    CHECK (numSlices (44100, 0, 0.0) == 0);

    // Non-dividing length: boundaries must not drift (rounded per slice)
    // BPM 174, 1/16, 44.1k -> 3801.7241... samples per slice
    const double sps = samplesPerSlice (44100.0, 174.0, divSixteenth);
    CHECK (sliceStart (1, 0, sps) == 3802);
    CHECK (sliceStart (2, 0, sps) == 7603); // llround(7603.44) - no accumulation
    CHECK (sliceStart (100, 0, sps) == 380172);

    // BMS base-36 numbering
    CHECK (bmsNumber (0) == "01");
    CHECK (bmsNumber (8) == "09");
    CHECK (bmsNumber (9) == "0A");
    CHECK (bmsNumber (34) == "0Z");
    CHECK (bmsNumber (35) == "10");
    CHECK (bmsNumber (1294) == "ZZ");

    if (failures == 0)
        std::printf ("ALL TESTS PASSED\n");
    return failures == 0 ? 0 : 1;
}
