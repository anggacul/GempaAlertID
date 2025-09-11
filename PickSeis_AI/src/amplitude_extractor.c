#include "core/amplitude_extractor.h"
#include "utils/logger.h"
#include <math.h>
#include "core/config.h"

typedef struct {
    double a0, a1, a2;
    double b1, b2;
    double z1, z2;
} Biquad;

static void setup_highpass(Biquad* f, double fs, double fc) {
    double c = tan(M_PI * fc / fs);
    double a0 = 1.0 + sqrt(2.0)*c + c*c;
    f->a0 = 1.0 / a0;
    f->a1 = -2.0 * f->a0;
    f->a2 = f->a0;
    f->b1 = 2.0 * (c*c - 1.0) * f->a0;
    f->b2 = (1.0 - sqrt(2.0)*c + c*c) * f->a0;
    f->z1 = f->z2 = 0.0;
}

static double filter_sample(Biquad* f, double x) {
    double y = f->a0*x + f->z1;
    f->z1 = f->a1*x - f->b1*y + f->z2;
    f->z2 = f->a2*x - f->b2*y;
    return y;
}

void butterworth_highpass(double* data, int n, double fs, double fc) {
    Biquad f;
    setup_highpass(&f, fs, fc);
    for (int i = 0; i < n; i++) {
        data[i] = filter_sample(&f, data[i]);
    }
}

// ---------------------------
// Integrasi trapezoidal
// ---------------------------
void integrate_array(const double* input, double* output, int n, double dt) {
    if (n <= 0) return;
    output[0] = 0.0; // asumsi kondisi awal nol
    for (int i = 1; i < n; i++) {
        output[i] = output[i-1] + 0.5 * (input[i] + input[i-1]) * dt;
    }
}

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
    // float maxAmp[3];

    float maxAmp;
    for (int i = pickIdx; i < endIdx; ++i) {
        float amp = fabsf(window->data[ch][i]);
        if (amp > maxAmp) maxAmp = amp;
    }
    return maxAmp;
}

float extractMaxAmplitudeAt(const Station* station, const DataWindow* window, double time, float maxAmp[3], float maxtime) {
    maxAmp[0] = maxAmp[1] = maxAmp[2] = 0.0f;
    float difftime;
    for (int ch = 0; ch < MAX_CHANNELS; ++ch) {
        
        int windowSamples = window->windowSamples[ch];
        double sampleRate = station->sampleRate;
        double dt = 1.0 / sampleRate;
        int idx = (int)((time - window->startTime[ch] - 1) * sampleRate);
        int endIdx = windowSamples;
        if (idx < 0){
            LOG_INFO("idx %d station %s max chan %d no channel %d starttime %.3f endtime %.3f",idx,station->stationId, MAX_CHANNELS, ch, window->startTime[ch], window->endTime[ch]);
            return 0.0;
        }
        
        difftime = (float)((endIdx - idx) / sampleRate);
        if (difftime > maxtime) {
            endIdx = idx + (int)(maxtime * sampleRate - 1);
            difftime = maxtime;
        }
        if (idx < 0) idx = 0;
        if (idx >= windowSamples) idx = windowSamples - 1;
        int ndata = endIdx - idx + 1;

        double acc[ndata], vel[ndata], disp[ndata];
        for (int i = 0; i < ndata; i++) {
            acc[i] = window->data[ch][idx + i];
        }
        integrate_array(acc, vel, ndata, dt);
        butterworth_highpass(vel, ndata, sampleRate, 0.1);

        integrate_array(vel, disp, ndata, dt);
        butterworth_highpass(disp, ndata, sampleRate, 0.075);

        for (int i = 0; i < ndata; i++) {
            float amp = fabsf(acc[i]);
            float ampvel = fabsf(vel[i]);
            float ampdisp = fabsf(disp[i]);
            if (amp > maxAmp[0]) maxAmp[0] = amp;
            if (ampvel > maxAmp[1]) maxAmp[1] = ampvel;
            if (ampdisp > maxAmp[2]) maxAmp[2] = ampdisp;
        }
    }
    return difftime;
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