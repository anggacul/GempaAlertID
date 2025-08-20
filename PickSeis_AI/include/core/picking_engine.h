#pragma once
#include "data_window.h"
#include "station_manager.h"
#include <stdbool.h>

/**
 * @brief Inisialisasi ONNX Runtime untuk PhaseNet
 * @return true jika sukses
 */
bool initONNXRuntime();

/**
 * @brief Hasil picking PhaseNet
 */
typedef struct {
    double pickTime;
    float confidence;
} PickResult;

/**
 * @brief Jalankan picking PhaseNet pada window data
 * @param station Pointer ke Station
 * @param window Pointer ke DataWindow
 * @return PickResult hasil picking
 */
PickResult runPhaseNetPicking(const Station* station, const DataWindow* window);

void cleanupONNXRuntime();

void downsample_to_20hz(const float* in, int in_samples, float* out, int out_samples, double in_sample_rate, double out_sample_rate); 