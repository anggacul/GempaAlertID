#pragma once
#include "config.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "data_window.h"

#define SM_MAX_CHANNELS 3
#define SM_MAX_STATION_ID_LEN 6
#define SM_MAX_CHANNEL_NAME_LEN 4

/**
 * @brief Station metadata and state
 * @struct Station
 * @var stationId ID stasiun
 * @var channels Daftar channel (3 komponen)
 * @var conversionFactor Faktor konversi per channel
 * @var sampleRate Sample rate (Hz)
 * @var lastPickTime Waktu picking terakhir
 * @var index Indeks station di array
 */
typedef struct Station {
    char stationId[SM_MAX_STATION_ID_LEN];
    char channels[SM_MAX_CHANNELS][SM_MAX_CHANNEL_NAME_LEN];
    double conversionFactor[SM_MAX_CHANNELS];
    double sampleRate; // sample rate (Hz)
    double lastPickTime;
    int index; // indeks station di array
    // Tambahkan metadata lain jika perlu
} Station;

typedef struct {
    double pickTime;
    float pickRms;
    int windowCountSincePick;
    int isWaitingAfterPick;
    int pickInfoSent;
    float lastConfidence;
    float Trms;
} PickState;

/**
 * @brief Proses utama untuk satu station
 * @param station Pointer ke struct Station
 */
void pad_copy(char *dest, const char *src, size_t width);
void processStation(Station* station, PickState* pickState);

/**
 * @brief Load daftar station dari file txt (format: station ch1 ch2 ch3 samplingrate conv1 conv2 conv3)
 * @param filename Nama file
 * @param stationList Array output
 * @param maxStation Jumlah maksimum station
 * @return Jumlah station yang berhasil dibaca
 */
int loadStationListFromFile(const char* filename, Station* stationList, int maxStation); 