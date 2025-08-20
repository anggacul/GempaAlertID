#include "core/data_window.h"
#include "utils/logger.h"
#include "seedlink_client.h"
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <sys/time.h>

/**
 * @brief Ambil snapshot window data terbaru dari SeedLink listener
 */
bool updateDataWindow(Station* station, DataWindow* window, uint64_t lastProcessedTimestamp) {
    // LOG_INFO("Update data window untuk station %s", station->stationId);
    return getLatestWindow(station, window, lastProcessedTimestamp);
}

/**
 * @brief Dummy update rolling window data 30 detik
 */
bool updateDataWindowDummy(Station* station, DataWindow* window) {
    // Dummy: isi data dengan noise, update waktu
    static double t = 0.0;
    for (int ch = 0; ch < MAX_CHANNELS; ++ch) {
        for (int i = 0; i < SAMPLES_PER_WINDOW; ++i) {
            window->data[ch][i] = (float)(0.1 * (rand() % 100) / 100.0);
        }
        window->startTime[ch] = t;
        window->endTime[ch] = t + 30.0;
    }
    t += 1.0; // rolling per detik
    struct timeval tv;
    gettimeofday(&tv, NULL);
    window->timestamp = (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
    return true;
} 