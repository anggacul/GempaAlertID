#pragma once
#include <stddef.h>
#include "core/config.h"

/**
 * @brief Interpolasi data pada gap kecil
 * @param data Array data
 * @param length Panjang data
 */
void interpolateGap(float* data, size_t length);

/**
 * @brief Interpolasi data pada gap kecil di rentang tertentu
 * @param data Array data
 * @param start Indeks awal (inklusif)
 * @param end Indeks akhir (inklusif)
 */
void interpolateGapRange(float* data, size_t start, size_t end); 