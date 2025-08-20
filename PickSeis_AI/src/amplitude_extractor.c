#include "core/amplitude_extractor.h"
#include "utils/logger.h"
#include <math.h>
#include "core/config.h"
#include "core/station_manager.h"
/**
 * @brief Ekstrak amplitudo maksimum 3 detik setelah pickTime
 */
float extractMaxAmplitude(const DataWindow* window, int ch, double pickTime) {
    int windowSamples = window->windowSamples[ch];
    if (windowSamples <= 0) windowSamples = SAMPLES_PER_WINDOW;
    double sampleRate = windowSamples / WW;
    double fixTime = window->endTime[ch]-10.0;
    int pickIdx = (int)((fixTime - window->startTime[ch]) * sampleRate);
    // int endIdx = pickIdx + (int)(3 * sampleRate);
    int endIdx = windowSamples;
    if (pickIdx < 0) pickIdx = 0;
    if (endIdx > windowSamples) endIdx = windowSamples;

    float maxAmp = 0.0f;
    for (int i = pickIdx; i < endIdx; ++i) {
        float amp = fabsf(window->data[ch][i]);
        if (amp > maxAmp) maxAmp = amp;
    }
    return maxAmp;
}

float extractMaxAmplitudeAt(const Station* station, const DataWindow* window, double time) {
    float maxAmp = 0.0f;
    for (int ch = 0; ch < MAX_CHANNELS; ++ch) {
        int windowSamples = window->windowSamples[ch];
        double sampleRate = station->sampleRate;
        int idx = (int)((time - window->startTime[ch]) * sampleRate);
        int endIdx = windowSamples;
        if (idx < 0) idx = 0;
        if (idx >= windowSamples) idx = windowSamples - 1;
        for (int i = idx; i < endIdx; ++i) {
            float amp = fabsf(window->data[ch][i]);
            if (amp > maxAmp) maxAmp = amp;
        }
    }
    return maxAmp;
}

float calculateRmsAmplitude(const DataWindow* window) {
    // RMS pada window sinkron (minLastTime)
    int minSamples = window->windowSamples[0];
    for (int ch = 1; ch < MAX_CHANNELS; ++ch) {
        if (window->windowSamples[ch] < minSamples) minSamples = window->windowSamples[ch];
    }
    if (minSamples <= 0) minSamples = SAMPLES_PER_WINDOW;
    double sumsq = 0.0;
    int n = 0;
    for (int i = 0; i < minSamples; ++i) {
        double sumch = 0.0;
        for (int ch = 0; ch < MAX_CHANNELS; ++ch) {
            sumch += window->data[ch][i] * window->data[ch][i];
        }
        sumsq += sqrt(sumch);
        n++;
    }
    if (n == 0) return 0.0f;
    return (float)sqrt(sumsq / n);
} 

float calculateRmsAmplitudeAt(const Station* station, const DataWindow* window) {
    // RMS pada window sinkron (minLastTime)
    double sampleRate = station->sampleRate;
    int pickWindowSamples = (int)(sampleRate * WW) + (int)(sampleRate / 50.0);
    double minLastTime = window->minLastTime;
    float temp[3][pickWindowSamples];
    for (int ch = 0; ch < 3; ++ch) {
        int windowSamples = window->windowSamples[ch];
        int endIdx = (int)((minLastTime - window->startTime[ch]) * sampleRate);
        int startIdx = endIdx - pickWindowSamples + 1;
        if (startIdx < 0) {
            startIdx = 0;
            endIdx = pickWindowSamples - 1;
        }
        // Tidak perlu memaksa endIdx ke windowSamples-1, rolling window tetap berjalan mundur
        if (endIdx >= windowSamples) {
            // Jika endIdx melebihi jumlah sample, rolling window tetap berjalan mundur
            endIdx = windowSamples - 1;
            startIdx = endIdx - pickWindowSamples + 1;
            if (startIdx < 0) startIdx = 0;
        }
        int n = 0;
        for (int i = startIdx; i <= endIdx && n < pickWindowSamples; ++i, ++n) {
            temp[ch][n] = window->data[ch][i];
        }
        // LOG_INFO("Station %s, ch=%d, timestamp=%.3f, minLastTime=%.3f, startTime=%.3f (idx=%d), endTime=%.3f (idx=%d)", station->stationId, ch, window->timestamp, minLastTime, window->startTime[ch], startIdx, (minLastTime - window->startTime[ch]), endIdx);
    }

    double maxrms = 0.0;
    for (int i = 0; i < pickWindowSamples; ++i) {
        double sumch = 0.0;
        for (int ch = 0; ch < MAX_CHANNELS; ++ch) {
            sumch += temp[ch][i] * temp[ch][i];
        }
        double sumsq = sqrt(sumch);
        if (sumsq > maxrms) maxrms = sumsq;
    }
    return maxrms;
} 