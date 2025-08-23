#pragma once
#include "data_window.h"

/**
 * @brief Ekstrak amplitudo maksimum 3 detik setelah pick
 * @param window Pointer ke DataWindow
 * @param pickTime Waktu pick
 * @return Amplitudo maksimum
 */
float extractMaxAmplitude(const DataWindow* window, int ch, double pickTime);

/**
 * @brief Hitung RMS amplitude window
 */
float calculateRmsAmplitude(const DataWindow* window);
float calculateRmsAmplitudeAt(const Station* station, const DataWindow* window);
/**
 * @brief Ekstrak amplitude pada waktu tertentu
 */
float extractMaxAmplitudeAt(const Station* station, const DataWindow* window, double time, float maxAmp[3]);