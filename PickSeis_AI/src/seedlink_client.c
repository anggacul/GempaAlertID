#include "seedlink_client.h"
#include <libslink.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <math.h>
#include <sys/time.h>
#include <time.h>
#include "core/config.h"
#include "utils/interpolation.h"
#include "utils/logger.h"
#include "core/station_manager.h"
#include "core/data_window.h"

static SLCD *slconn;
SLpacket *slpack;
static DataWindow g_windows[MAX_STATIONS];
static Station* g_stationList = NULL;
static int g_stationCount = 0;
static pthread_mutex_t g_window_mutex[MAX_STATIONS];
static double g_lastSampleTime[MAX_STATIONS][MAX_CHANNELS] = {{0}};
static int slRecordSize = 512;
static int seqnum;
static int ptype;
static int retval;
static int packetcnt = 0;
static int stateint = 100;

void trim_spaces(char *str) {
    // Trim leading spaces
    char *start = str;
    while (*start == ' ') start++;
    if (start != str) memmove(str, start, strlen(start) + 1);

    // Trim trailing spaces
    char *end = str + strlen(str) - 1;
    while (end >= str && *end == ' ') {
        *end = '\0';
        end--;
    }
}

static int find_station_index(const char* stationId) {
    for (int i = 0; i < g_stationCount; ++i) {
        if (strncmp(g_stationList[i].stationId, stationId, MAX_STATION_ID_LEN) == 0) {
            return i;
        }
    }
    return -1;
}

static int find_channel_index(const Station* station, const char* channel) {
    for (int ch = 0; ch < MAX_CHANNELS; ++ch) {
        if (strncmp(station->channels[ch], channel, MAX_CHANNEL_NAME_LEN) == 0) {
            return ch;
        }
    }
    return -1;
}

static float calculateMean(const float* data, int n) {
    if (n <= 0) return 0.0f;
    double sum = 0.0;
    for (int i = 0; i < n; ++i) sum += data[i];
    return (float)(sum / n);
}

static void* seedlink_listener_thread(void* arg) {
    while ((retval = sl_collect_nb_size (slconn, &slpack, slRecordSize))) {
        if ( retval == SLPACKET ){
            ptype  = sl_packettype (slpack);
            seqnum = sl_sequence (slpack);
            if (ptype == SLDATA) {
                SLMSrecord * msr = NULL;
                sl_msr_parse_size (slconn->log, (char *) &slpack->msrecord, &msr, 1, 1, slRecordSize);
                if (!msr) {
                    LOG_INFO("msr is NULL");
                    continue;
                }
                else if (msr->numsamples == -1) {
                    LOG_INFO("msr->numsamples is -1");
                    sl_msr_free(&msr);
                    msr = NULL;
                    continue;
                }

                char station[6];  
                char channel[4];
                strncpy(station, msr->fsdh.station, 5);
                station[5] = '\0';
                trim_spaces(station);
                strncpy(channel, msr->fsdh.channel, 3);
                channel[3] = '\0';
                trim_spaces(channel);
                int idx = find_station_index(station);
                // LOG_INFO("find_station_index for station %s %d", station, idx);
                // LOG_INFO("find_station_index for station %zu %zu %d %d", strlen(g_stationList[0].stationId), strlen(station), xxx, MAX_STATION_ID_LEN);
                if (idx >= 0) {
                    pthread_mutex_lock(&g_window_mutex[idx]);
                    int chidx = find_channel_index(&g_stationList[idx], channel);
                    if (chidx >= 0 && msr->datasamples && msr->numsamples > 0) {
                        // LOG_INFO("station %s channel %s", station, channel);
                        double sampleRate = g_stationList[idx].sampleRate;
                        int windowSamples = SAMPLES_PER_WINDOW;
                        int shiftSamples = windowSamples;
                        double sampleInterval = 1.0 / sampleRate;
                        double expectedStart = g_lastSampleTime[idx][chidx] + sampleInterval;
                        double actualStart = sl_msr_depochstime(msr);
                        double gap = actualStart - expectedStart;
                        // if (strcmp(channel, "HNE") == 0) {
                        //     LOG_INFO("station %s channel %s, gap=%.3f, actualStart=%.3f", station, channel, gap, actualStart);
                        // }
                        // Deteksi overlap
                        double lastTime = g_lastSampleTime[idx][chidx];
                        double firstTime = actualStart;
                        int overlapSamples = 0;
                        if (lastTime >= firstTime) {
                            overlapSamples = (int)round((lastTime - firstTime) * sampleRate) + 1;
                            if (overlapSamples >= msr->numsamples) {
                                pthread_mutex_unlock(&g_window_mutex[idx]);
                                continue;
                            }
                        }
                        int dataOffset = (overlapSamples > 0) ? overlapSamples : 0;
                        int newSamples = msr->numsamples - dataOffset;
                        // if (newSamples > shiftSamples) {
                        //     dataOffset = msr->numsamples - shiftSamples;
                        //     newSamples = shiftSamples;
                        // }
                        int32_t* intdata = (int32_t*)msr->datasamples;

                        // Gap/interpolasi
                        if (g_lastSampleTime[idx][chidx] > 0) {
                            if (gap < MAX_GAP && gap > 1.0*(sampleInterval)) {
                                // LOG_INFO("Gap kecil pada station %s channel %s: %.2f detik", g_stationList[idx].stationId, channel, gap);
                                int gapSamples = (int)round(gap / sampleInterval);
                                float* chdata = g_windows[idx].data[chidx];
                                int currentSamples = g_windows[idx].windowSamples[chidx];
                                double conv = g_stationList[idx].conversionFactor[chidx];
                                if (gapSamples > 0 && newSamples > 0) {
                                    float endVal = (float)intdata[dataOffset] * conv - g_windows[idx].lastMean[chidx];
                                    if (currentSamples >= SAMPLES_PER_WINDOW) {
                                        // Geser buffer ke kiri sebanyak gapSamples
                                        memmove(&chdata[0], &chdata[gapSamples], sizeof(float) * (SAMPLES_PER_WINDOW - gapSamples));
                                        currentSamples = SAMPLES_PER_WINDOW - gapSamples;
                                        // Interpolasi antara nilai terakhir buffer lama dan data baru pertama
                                        float startVal = chdata[currentSamples - 1];
                                        for (int g = 1; g <= gapSamples; ++g) {
                                            chdata[currentSamples - 1 + g] = startVal + (endVal - startVal) * g / (gapSamples + 1);
                                        }
                                        currentSamples += gapSamples;
                                        g_windows[idx].windowSamples[chidx] = currentSamples;
                                    } else if (currentSamples > 0 && currentSamples + gapSamples < SAMPLES_PER_WINDOW) {
                                        float startVal = chdata[currentSamples - 1];
                                        for (int g = 1; g <= gapSamples; ++g) {
                                            chdata[currentSamples - 1 + g] = startVal + (endVal - startVal) * g / (gapSamples + 1);
                                        }
                                        currentSamples += gapSamples;
                                        g_windows[idx].windowSamples[chidx] = currentSamples;
                                    }
                                }
                            } else if (fabs(gap) >= MAX_GAP) {
                                memset(g_windows[idx].data[chidx], 0, sizeof(float) * windowSamples);
                                g_windows[idx].windowSamples[chidx] = 0;
                                g_windows[idx].startTime[chidx] = 0;
                                g_windows[idx].endTime[chidx] = 0;
                                g_windows[idx].full[chidx] = 0;
                                g_windows[idx].lastMean[chidx] = 0;
                                // LOG_INFO("Gap besar pada station %s channel %s: %.2f detik, buffer direset", g_stationList[idx].stationId, channel, gap);
                            }
                        }
                        
                        if (intdata) {
                            float* chdata = g_windows[idx].data[chidx];
                            int currentSamples = g_windows[idx].windowSamples[chidx];
                            double conv = g_stationList[idx].conversionFactor[chidx];
                            g_windows[idx].lastMean[chidx] = calculateMean(chdata, currentSamples);
                            if (currentSamples < SAMPLES_PER_WINDOW) {
                                int space = SAMPLES_PER_WINDOW - currentSamples;
                                int toCopy = (newSamples < space) ? newSamples : space;
                                for (int i = 0; i < toCopy; ++i) {
                                    // chdata[currentSamples + i] = (float)intdata[dataOffset + i] * conv;
                                    chdata[currentSamples + i] = (float)intdata[dataOffset + i] * conv - g_windows[idx].lastMean[chidx];
                                }
                                currentSamples += toCopy;
                                if (newSamples > space) {
                                    int roll = newSamples - space;
                                    memmove(&chdata[0], &chdata[roll], sizeof(float) * (SAMPLES_PER_WINDOW - roll));
                                    for (int i = 0; i < roll; ++i) {
                                        chdata[SAMPLES_PER_WINDOW - roll + i] = (float)intdata[dataOffset + space + i] * conv - g_windows[idx].lastMean[chidx];
                                    }
                                    currentSamples = SAMPLES_PER_WINDOW;
                                }
                                g_windows[idx].windowSamples[chidx] = currentSamples;
                            } else {                               
                                memmove(&chdata[0], &chdata[newSamples], sizeof(float) * (SAMPLES_PER_WINDOW - newSamples));
                                for (int i = 0; i < newSamples; ++i) {
                                    chdata[SAMPLES_PER_WINDOW - newSamples + i] = (float)intdata[dataOffset + i] * conv - g_windows[idx].lastMean[chidx];
                                }
                                g_windows[idx].windowSamples[chidx] = SAMPLES_PER_WINDOW;
                            }
                            free(intdata);
                        }
                        if (g_windows[idx].windowSamples[chidx] >= WW * sampleRate){
                            g_windows[idx].full[chidx] = 1;// LOG_INFO("station %s window full, newSamples=%d gap=%f", g_stationList[idx].stationId, newSamples, gap);
                        }
                        double endTime = actualStart + (msr->numsamples - 1) * sampleInterval;
                        g_windows[idx].endTime[chidx] = endTime;
                        g_windows[idx].startTime[chidx] = endTime - g_windows[idx].windowSamples[chidx] * sampleInterval;
                        // g_windows[idx].windowSamples[chidx] = windowSamples;
                        // Update minLastTime untuk sinkronisasi picking
                        double minLastTime = g_windows[idx].endTime[0];
                        for (int c = 1; c < MAX_CHANNELS; ++c) {
                            if (g_windows[idx].endTime[c] < minLastTime) {
                                minLastTime = g_windows[idx].endTime[c];
                            }
                        }
                        g_windows[idx].minLastTime = minLastTime;
                        struct timeval tv;
                        gettimeofday(&tv, NULL);
                        g_windows[idx].timestamp = tv.tv_sec + tv.tv_usec / 1e6;
                        g_lastSampleTime[idx][chidx] = endTime;
                    }
                    pthread_mutex_unlock(&g_window_mutex[idx]);
                }
            }
            if ( STATEFILE && stateint )
            {
                if ( ++packetcnt >= stateint )
                {
                    sl_savestate (slconn, STATEFILE);
                    packetcnt = 0;
                }
            }

        }
        else{
            usleep(5000);
        }
        // sl_msr_free(&slpack->msrecord);
        // free(slpack);
    }
    if ( slconn->link != -1 ) sl_disconnect (slconn);
    if ( STATEFILE ) sl_savestate (slconn, STATEFILE);
    // Setelah loop utama (disconnect/error)
    LOG_ERROR("SeedLink connection lost (slconn disconnect). Exiting main loop.");
    extern volatile int keepRunning;
    keepRunning = 0;
    return NULL;
}

bool startSeedLinkListener(const char* host, Station* stationList, int stationCount, char* statefile) {
    g_stationList = stationList;
    g_stationCount = stationCount;
    for (int i = 0; i < stationCount; ++i) {
        pthread_mutex_init(&g_window_mutex[i], NULL);
    }
    slconn = sl_newslcd();
    slconn->netto = atoi (strdup("600"));
    slconn->netdly = atoi (strdup("30"));
    slconn->keepalive = atoi (strdup("0"));

    for (int i = 0; i < stationCount; ++i) {
        sl_addstream(slconn, "IA", stationList[i].stationId, NULL, -1, NULL);
    }
    slconn->sladdr = strdup(host);
    if (STATEFILE) {
        if (sl_recoverstate (slconn, STATEFILE) < 0) {
            sl_log (1, 0, "state recovery failed\n");
        }
    }
    if (sl_connect(slconn, 0) < 0) return false;
    pthread_t tid;
    pthread_create(&tid, NULL, seedlink_listener_thread, NULL);
    return true;
}

bool getLatestWindow(Station* station, DataWindow* window, double lastProcessedTimestamp) {
    int idx = find_station_index(station->stationId);
    if (idx < 0) return false;
    pthread_mutex_lock(&g_window_mutex[idx]);
    if (g_windows[idx].timestamp <= lastProcessedTimestamp) {
        pthread_mutex_unlock(&g_window_mutex[idx]);
        return false;
    }
    memcpy(window, &g_windows[idx], sizeof(DataWindow));
    pthread_mutex_unlock(&g_window_mutex[idx]);
    return true;
}

void cleanupSeedLink() {
    if (slconn) {
        if (slconn->link != -1) sl_disconnect(slconn);
        if (STATEFILE) sl_savestate(slconn, STATEFILE);
        sl_terminate(slconn);
        sl_disconnect(slconn);
        sl_freeslcd(slconn);
        slconn = NULL;
    }
    // Optional: join thread, destroy mutex jika perlu
} 