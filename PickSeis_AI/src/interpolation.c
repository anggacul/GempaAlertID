#include "utils/interpolation.h"
#include "utils/logger.h"

/**
 * @brief Interpolasi linear sederhana (dummy)
 */
void interpolateGap(float* data, size_t length) {
    // Interpolasi linear sederhana pada gap (nilai 0 dianggap gap)
    size_t i = 0;
    while (i < length) {
        // Cari awal gap
        while (i < length && data[i] != 0.0f) i++;
        size_t gapStart = (i > 0) ? i - 1 : 0;
        // Cari akhir gap
        while (i < length && data[i] == 0.0f) i++;
        size_t gapEnd = i;
        // Interpolasi jika gap diapit dua titik valid
        if (gapStart < length && gapEnd < length && gapEnd > gapStart + 1) {
            float startVal = data[gapStart];
            float endVal = data[gapEnd];
            size_t gapLen = gapEnd - gapStart - 1;
            for (size_t j = 1; j <= gapLen; ++j) {
                data[gapStart + j] = startVal + (endVal - startVal) * j / (gapLen + 1);
            }
        }
    }
}

void interpolateGapRange(float* data, size_t start, size_t end) {
    // Interpolasi linear sederhana pada gap (nilai 0 dianggap gap) di rentang tertentu
    size_t i = start;
    while (i <= end) {
        // Cari awal gap
        while (i <= end && data[i] != 0.0f) i++;
        size_t gapStart = (i > start) ? i - 1 : start;
        // Cari akhir gap
        while (i <= end && data[i] == 0.0f) i++;
        size_t gapEnd = i;
        // Interpolasi jika gap diapit dua titik valid
        if (gapStart < end && gapEnd <= end && gapEnd > gapStart + 1) {
            float startVal = data[gapStart];
            float endVal = data[gapEnd];
            size_t gapLen = gapEnd - gapStart - 1;
            for (size_t j = 1; j <= gapLen; ++j) {
                data[gapStart + j] = startVal + (endVal - startVal) * j / (gapLen + 1);
            }
        }
    }
}