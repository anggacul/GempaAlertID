#include "core/config.h"
#include "core/station_manager.h"
#include "core/data_window.h"
#include "core/picking_engine.h"
#include "core/amplitude_extractor.h"
#include "utils/logger.h"
#include "utils/bandpass_filter.h"
#include "utils/sqlite_writer.h"
#include <string.h>
#include <stdio.h>
#include <sys/time.h>
#include <stddef.h>

/**
 * @brief State untuk kontrol picking per station
 */
void pad_copy(char *dest, const char *src, size_t width) {
    size_t len = strlen(src);
    if (len > width) len = width;
    memcpy(dest, src, len);
    for (size_t i = len; i < width; ++i) dest[i] = ' ';
    dest[width] = '\0'; // null-terminate
}
static bool status_window = false;
int loadStationListFromFile(const char* filename, Station* stationList, int maxStation) {
    FILE* f = fopen(filename, "r");
    if (!f) return 0;
    int count = 0;
    char line[256];
    while (fgets(line, sizeof(line), f) && count < maxStation) {
        char stid[MAX_STATION_ID_LEN], ch1[MAX_CHANNEL_NAME_LEN], ch2[MAX_CHANNEL_NAME_LEN], ch3[MAX_CHANNEL_NAME_LEN];
        double sr, conv1, conv2, conv3;
        int n = sscanf(line, "%15s %7s %7s %7s %lf %lf %lf %lf", stid, ch1, ch2, ch3, &sr, &conv1, &conv2, &conv3);
        if (n == 8) {
            strncpy(stationList[count].stationId, stid, MAX_STATION_ID_LEN);
            strncpy(stationList[count].channels[0], ch1, MAX_CHANNEL_NAME_LEN);
            strncpy(stationList[count].channels[1], ch2, MAX_CHANNEL_NAME_LEN);
            strncpy(stationList[count].channels[2], ch3, MAX_CHANNEL_NAME_LEN);
            stationList[count].sampleRate = sr;
            stationList[count].conversionFactor[0] = 1.0/conv1;
            stationList[count].conversionFactor[1] = 1.0/conv2;
            stationList[count].conversionFactor[2] = 1.0/conv3;
            stationList[count].lastPickTime = 0.0;
            stationList[count].index = count;
            // LOG_INFO("station %s %d %zu %zu", stationList[count].stationId, MAX_STATION_ID_LEN, strlen(stationList[count].stationId), strlen(stid));
            count++;
        }
    }
    fclose(f);
    return count;
}

void removeMean(float* data, int n) {
    if (n <= 0) return;
    double sum = 0.0;
    for (int i = 0; i < n; ++i) sum += data[i];
    float mean = (float)(sum / n);
    for (int i = 0; i < n; ++i) data[i] -= mean;
}

void write_to_shared_memory(Station* station, PickState* pickState, float amp) {
    // Memperbarui counter
    current_counter++;

    // Menulis data ke shared memory
    ptr->counter = current_counter;
    sprintf(ptr->data, "[PICK] station %s pada %.2f RMS=%.3f, amp@Tt=%.3f", station->stationId, pickState->pickTime, pickState->pickRms, amp);

    LOG_INFO("%s", ptr->data);

    // Menaikkan semafor untuk memberi sinyal ke proses lain
    if (sem_post(sem) == -1) {
        LOG_ERROR("sem_post failed");
        perror("sem_post failed");
        // Pertimbangkan penanganan error lebih lanjut jika diperlukan
    }
}

/**
 * @brief Proses utama untuk satu station (loop real-time)
 */
void processStation(Station* station, PickState* pickState) {
    static float last_confidence[MAX_STATIONS] = {0};
    static double lastProcessedTimestamp[MAX_STATIONS] = {0};
    DataWindow window = {0};
    int idx = station->index;
    status_window = updateDataWindow(station, &window, lastProcessedTimestamp[idx]);
    if (!status_window) {
        return;
    }
    struct timeval tv;
    gettimeofday(&tv, NULL);
    double sysnow = tv.tv_sec + tv.tv_usec / 1e6;
    // 2. Preprocessing: DC offset removal (rmean) dan bandpass filter
    double now = window.minLastTime; // gunakan waktu sinkron
    int windowSamples = 0;
    for (int ch = 0; ch < MAX_CHANNELS; ++ch) {
        windowSamples = window.windowSamples[ch];
    }

    // 3. Cek apakah ada pick yang harus diinfokan (Tt detik setelah pick)
    if (pickState->isWaitingAfterPick && !pickState->pickInfoSent) {
        if (window.minLastTime >= pickState->pickTime + PICK_TT) {
            float amp = extractMaxAmplitudeAt(station, &window, pickState->pickTime + PICK_TT);
            // LOG_INFO("[PICK] station %s pada %.2f (RMS=%.3f, amp@Tt=%.3f, timestamp=%.3f, minLastTime=%.3f)", station->stationId, pickState->pickTime, pickState->pickRms, amp, window.timestamp, window.minLastTime);
            write_to_shared_memory(station, pickState, amp);
            sqlite_insert_pick(station->stationId, pickState->pickTime, amp, pickState->lastConfidence);
            // if (window.minLastTime >= pickState->pickTime + 9) {
            pickState->pickInfoSent = 1;
            // }
        }
    }
    // 4. Jika sedang menunggu setelah pick
    if (pickState->isWaitingAfterPick) {
        float rms = calculateRmsAmplitudeAt(station, &window);
        if (rms >= pickState->pickRms) {
            pickState->pickRms = rms;
        } else {
            if (pickState->Trms == 0.0) {
                pickState->Trms = 0.2 * pickState->pickRms;
                if (pickState->Trms < 1e-7) {
                    pickState->Trms = 0.0;
                    pickState->isWaitingAfterPick = 0;
                    // LOG_INFO("Station %s reset picking", station->stationId);
                }
            }
            if ( rms < 0.2f * pickState->pickRms) {
                pickState->windowCountSincePick++;pickState->isWaitingAfterPick = 0; // aktifkan picking lagi
                // LOG_INFO("Station %s reset picking", station->stationId);
            }
        }
        if (pickState->windowCountSincePick >= PICK_NT) {
            pickState->isWaitingAfterPick = 0;
            // LOG_INFO("Station %s reset picking", station->stationId);
        }
        if (window.minLastTime > pickState->pickTime + 360){
            pickState->isWaitingAfterPick = 0;
            // LOG_INFO("Station %s reset picking", station->stationId);            
        }
    }
    // 5. Picking baru
    if (status_window && !pickState->isWaitingAfterPick && window.full[0] && window.full[1] && window.full[2]) {
        PickResult pick = runPhaseNetPicking(station, &window);
        if (pick.confidence > 0.5) {
            pickState->pickTime = pick.pickTime;
            pickState->pickRms = calculateRmsAmplitudeAt(station, &window);
            pickState->windowCountSincePick = 0;
            pickState->isWaitingAfterPick = 1;
            pickState->pickInfoSent = 0;
            pickState->lastConfidence = pick.confidence;
            pickState->Trms = 0.0;
            // (jangan langsung kirim, tunggu Tt detik)
        }
    }
    lastProcessedTimestamp[idx] = window.timestamp;
} 