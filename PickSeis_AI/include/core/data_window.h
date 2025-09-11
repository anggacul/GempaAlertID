#pragma once
#include "core/config.h"
#include <stdbool.h>
#include <stdint.h>

#define MAX_CHANNELS 3
#define SM_SAMPLES_PER_WINDOW 6000

struct Station;
typedef struct Station Station;

struct BiquadHPF;
typedef struct BiquadHPF BiquadHPF;

/**
 * @brief Buffer data rolling window WW detik
 */
typedef struct {
    double data[MAX_CHANNELS][SM_SAMPLES_PER_WINDOW];
    double datavel[MAX_CHANNELS][SM_SAMPLES_PER_WINDOW];
    double datadisp[MAX_CHANNELS][SM_SAMPLES_PER_WINDOW];
    double startTime[MAX_CHANNELS];   // waktu sample pertama per channel
    double endTime[MAX_CHANNELS];     // waktu sample terakhir per channel
    int windowSamples[MAX_CHANNELS];  // jumlah sample per channel
    double minLastTime;                  // waktu paling akhir yang tersedia di semua channel (sinkronisasi)
    double lastMean[MAX_CHANNELS];     // mean window per channel
    double lastMeanvel[MAX_CHANNELS];     // mean window per channel
    double lastMeandisp[MAX_CHANNELS];     // mean window per channel
    double timestamp;   // waktu update terakhir (epoch detik, bisa pecahan)
    int full[MAX_CHANNELS];
} DataWindow;

/**
 * @brief Update data window untuk satu station jika ada data baru
 * @param station Pointer ke Station
 * @param window Pointer ke DataWindow
 * @param lastProcessedTimestamp Timestamp terakhir yang sudah diproses
 * @return true jika ada data baru (timestamp lebih besar dari lastProcessedTimestamp)
 */
bool updateDataWindow(Station* station, DataWindow* window, double lastProcessedTimestamp); 