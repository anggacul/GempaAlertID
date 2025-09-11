#pragma once
#include "data_window.h"
#include "core/station_manager.h"

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
float extractMaxAmplitudeAt(const Station* station, const DataWindow* window, double time, float maxAmp[3], float maxtime);
// High-pass Butterworth orde 2 untuk array
void butterworth_highpass(double* data, int n, double fs, double fc);

// Integrasi trapezoidal untuk array
// acc -> vel (pertama), vel -> disp (kedua)
void integrate_array(const double* input, double* output, int n, double dt);