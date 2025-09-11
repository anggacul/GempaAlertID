#pragma once
#include "config.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "data_window.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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

typedef struct BiquadHPF {
    double b0, b1, b2;
    double a1, a2;
    double z1, z2;
} BiquadHPF;

typedef struct Station {
    char stationId[MAX_STATION_ID_LEN];
    char channels[MAX_CHANNELS][MAX_CHANNEL_NAME_LEN];
    double conversionFactor[MAX_CHANNELS];
    double sampleRate; // sample rate (Hz)
    double lastPickTime;
    int index; // indeks station di array
    BiquadHPF hpf_acc[MAX_CHANNELS];
    BiquadHPF hpf_vel[MAX_CHANNELS];
    BiquadHPF hpf_disp[MAX_CHANNELS];
    // Tambahkan metadata lain jika perlu
} Station;

typedef struct {
    double pickTime;
    float pickRms;
    int windowCountSincePick;
    int isWaitingAfterPick;
    int pickInfoSent;
    int pickSendSQL;
    int pickSendLOG;
    float upd_sec;
    float lastConfidence;
    float Trms;
} PickState;

/**
 * @brief Proses utama untuk satu station
 * @param station Pointer ke struct Station
 */
void pad_copy(char *dest, const char *src, size_t width);
void processStation(Station* station, PickState* pickState, double *lastProcessedTimestamp);

/**
 * @brief Load daftar station dari file txt (format: station ch1 ch2 ch3 samplingrate conv1 conv2 conv3)
 * @param filename Nama file
 * @param stationList Array output
 * @param maxStation Jumlah maksimum station
 * @return Jumlah station yang berhasil dibaca
 */
int loadStationListFromFile(const char* filename, Station* stationList, int maxStation); 
void write_to_shared_memory(Station* station, PickState* pickState, DataWindow* window, float amp[3], float upd_sec);
double biquad_hpf_step(BiquadHPF *f, double x);
void biquad_hpf_design(BiquadHPF *f, double fs, double fc);