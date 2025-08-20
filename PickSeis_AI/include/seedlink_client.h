#pragma once
#include "core/station_manager.h"
#include "core/data_window.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern volatile int keepRunning;

#ifdef __cplusplus
}
#endif

/**
 * @brief Inisialisasi dan mulai listener SeedLink (thread)
 * @param host Host SeedLink (misal: "127.0.0.1:18000")
 * @param stationList Daftar station
 * @param stationCount Jumlah station
 * @return true jika sukses
 */
bool startSeedLinkListener(const char* host, Station* stationList, int stationCount, char* statefile);

/**
 * @brief Ambil data window terbaru untuk satu station jika ada data baru
 * @param station Pointer ke Station
 * @param window Pointer ke DataWindow
 * @param lastProcessedTimestamp Timestamp terakhir yang sudah diproses
 * @return true jika ada data baru (timestamp lebih besar dari lastProcessedTimestamp)
 */
bool getLatestWindow(Station* station, DataWindow* window, double lastProcessedTimestamp);

void cleanupSeedLink(); 

void trim_spaces(char *str);