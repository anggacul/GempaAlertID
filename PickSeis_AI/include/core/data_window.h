#pragma once
#include "core/config.h"
#include <stdbool.h>
#include <stdint.h>

#define SM_MAX_CHANNELS 3
#define SM_SAMPLES_PER_WINDOW 6000

struct Station;
typedef struct Station Station;

struct BiquadHPF;
typedef struct BiquadHPF BiquadHPF;

/**
 * @brief Buffer data rolling window WW detik
 */
typedef struct {
    float data[SM_MAX_CHANNELS][SM_SAMPLES_PER_WINDOW];
    float datavel[SM_MAX_CHANNELS][SM_SAMPLES_PER_WINDOW];
    float datadisp[SM_MAX_CHANNELS][SM_SAMPLES_PER_WINDOW];
    double startTime[SM_MAX_CHANNELS];   // waktu sample pertama per channel
    double endTime[SM_MAX_CHANNELS];     // waktu sample terakhir per channel
    int windowSamples[SM_MAX_CHANNELS];  // jumlah sample per channel
    double minLastTime;                  // waktu paling akhir yang tersedia di semua channel (sinkronisasi)
    float lastMean[SM_MAX_CHANNELS];     // mean window per channel
    float lastMeanvel[SM_MAX_CHANNELS];     // mean window per channel
    float lastMeandisp[SM_MAX_CHANNELS];     // mean window per channel
    double timestamp;   // waktu update terakhir (epoch detik, bisa pecahan)
    int full[SM_MAX_CHANNELS];
} DataWindow;

/**
 * @brief Update data window untuk satu station jika ada data baru
 * @param station Pointer ke Station
 * @param window Pointer ke DataWindow
 * @param lastProcessedTimestamp Timestamp terakhir yang sudah diproses
 * @return true jika ada data baru (timestamp lebih besar dari lastProcessedTimestamp)
 */
bool updateDataWindow(Station* station, DataWindow* window, double lastProcessedTimestamp); 