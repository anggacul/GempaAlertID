#include "core/picking_engine.h"
#include "utils/logger.h"
#include <stdlib.h>
#include <onnxruntime_c_api.h>
#include <math.h>
#include "core/config.h"

// Path model ONNX (harus disesuaikan)

static OrtEnv* ort_env = NULL;
static OrtSession* ort_session = NULL;
static int onnx_initialized = 0;

bool initONNXRuntime() {
    if (onnx_initialized) return true;
    const OrtApi* api = OrtGetApiBase()->GetApi(ORT_API_VERSION);
    OrtStatus* status = NULL;
    status = api->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "phasenet", &ort_env);
    if (status != NULL) return false;
    OrtSessionOptions* session_options = NULL;
    api->CreateSessionOptions(&session_options);
    api->SetIntraOpNumThreads(session_options, 1);
    status = api->CreateSession(ort_env, PHASENET_ONNX_PATH, session_options, &ort_session);
    api->ReleaseSessionOptions(session_options);
    if (status == NULL) { onnx_initialized = 1; return true; }
    return false;
}

void cleanupONNXRuntime() {
    const OrtApi* api = OrtGetApiBase()->GetApi(ORT_API_VERSION);
    if (ort_session) {
        api->ReleaseSession(ort_session);
        ort_session = NULL;
    }
    if (ort_env) {
        api->ReleaseEnv(ort_env);
        ort_env = NULL;
    }
    onnx_initialized = 0;
}

// Downsampling sederhana ke 20 Hz (decimation)
void downsample_to_20hz(const float* in, int in_samples, float* out, int out_samples, double in_sample_rate, double out_sample_rate) {
    double ratio = in_sample_rate / out_sample_rate;
    for (int i = 0; i < out_samples; ++i) {
        int idx = (int)round(i * ratio);
        if (idx >= in_samples) idx = in_samples - 1;
        out[i] = in[idx];
    }
}

/**
 * @brief Dummy PhaseNet picking (random pick)
 */
PickResult runPhaseNetPicking(const Station* station, const DataWindow* window) {
    if (!initONNXRuntime()) {
        LOG_ERROR("ONNX PhaseNet belum terinisialisasi!");
        PickResult result = {0};
        return result;
    }

    PickResult result = {0};
    int phasenet_samples = (int)(50.0 * WW) + 1; // ex: 3001
    size_t per_ch = (size_t)phasenet_samples;
    size_t nvals = 3 * per_ch;

    /* Alloc buffers on heap (calloc so remaining entries = 0) */
    float *downsampled = (float*)calloc(nvals, sizeof(float));
    float *normed = (float*)malloc(nvals * sizeof(float));
    float *input_tensor = (float*)malloc(nvals * sizeof(float));
    if (!downsampled || !normed || !input_tensor) {
        LOG_ERROR("Malloc failure in runPhaseNetPicking");
        free(downsampled); free(normed); free(input_tensor);
        return result;
    }

    double minLastTime = window->minLastTime;

    /* Per-channel: build temp buffer, downsample (or copy) into downsampled */
    for (int ch = 0; ch < MAX_CHANNELS; ++ch) {
        int windowSamples = window->windowSamples[ch];
        double sampleRate = station->sampleRate;
        int pickWindowSamples = (int)(sampleRate * WW) + (int)(sampleRate / 50.0);
        if (windowSamples < pickWindowSamples){
            // LOG_INFO("Station %s channel %s data belum cukup untuk picking", station->stationId, station->channels[ch]);
            return result;
        }
        int endIdx = (int)((minLastTime - window->startTime[ch]) * sampleRate -1);
        int startIdx = endIdx - pickWindowSamples;
        if (startIdx < 0) {
            // LOG_INFO("Station %s channel %s samples %d data belum cukup untuk picking", station->stationId, station->channels[ch], windowSamples);
            return result;
            // startIdx = 0;
            // endIdx = pickWindowSamples - 1;
        }
        if (endIdx > windowSamples) {
            LOG_INFO("Station %s channel %s lastdata tidak sama startidx %d endidx %d samples %d pws %d", station->stationId, station->channels[ch], startIdx, endIdx, windowSamples, pickWindowSamples);
            return result;
            // endIdx = windowSamples - 1;
            // startIdx = endIdx - pickWindowSamples + 1;
            // if (startIdx < 0) startIdx = 0;
        }

        /* allocate temp on heap (pickWindowSamples can be large) */
        float *temp = (float*)malloc(sizeof(float) * (size_t)pickWindowSamples);
        if (!temp) {
            LOG_ERROR("Malloc failure for temp buffer");
            free(downsampled); free(normed); free(input_tensor);
            return result;
        }

        int n = 0;
        for (int i = startIdx; i <= endIdx && n < pickWindowSamples; ++i, ++n) {
            temp[n] = window->data[ch][i];
        }

        /* place result into downsampled[ch * per_ch + ...] */
        float *dst = downsampled + (size_t)ch * per_ch;
        if (station->sampleRate == 50.0) {
            /* copy n samples; rest already zero because of calloc */
            memcpy(dst, temp, sizeof(float) * (size_t)n);
        } else {
            downsample_to_20hz(temp, n, dst, phasenet_samples, sampleRate, 50.0);
        }

        free(temp);
    }

    /* Normalisasi terhadap max */
    float maxval = 0.0f;
    for (size_t k = 0; k < nvals; ++k) {
        float v = fabsf(downsampled[k]);
        if (v > maxval) maxval = v;
    }
    if (maxval < 1e-6f) maxval = 1.0f;

    for (size_t k = 0; k < nvals; ++k) normed[k] = downsampled[k] / maxval;

    /* Siapkan input tensor (heap) */
    for (size_t k = 0; k < nvals; ++k) input_tensor[k] = normed[k];

    /* ONNX inference (dengan cek error dan cleanup) */
    const OrtApi* api = OrtGetApiBase()->GetApi(ORT_API_VERSION);
    OrtStatus* status = NULL;
    OrtMemoryInfo* mem_info = NULL;

    status = api->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault, &mem_info);
    if (status != NULL) {
        LOG_ERROR("CreateCpuMemoryInfo failed: %s", api->GetErrorMessage(status));
        api->ReleaseStatus(status);
        goto cleanup_heap;
    }

    int64_t input_shape[3] = {1, 3, phasenet_samples};
    OrtValue* input_tensor_ort = NULL;
    size_t input_bytes = nvals * sizeof(float);

    status = api->CreateTensorWithDataAsOrtValue(mem_info, input_tensor, input_bytes,
                                                 input_shape, 3, ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT,
                                                 &input_tensor_ort);
    if (status != NULL) {
        LOG_ERROR("CreateTensorWithDataAsOrtValue failed: %s", api->GetErrorMessage(status));
        api->ReleaseStatus(status);
        api->ReleaseMemoryInfo(mem_info);
        goto cleanup_heap;
    }
    api->ReleaseMemoryInfo(mem_info);  /* safe to release after CreateTensorWithDataAsOrtValue */

    const char* input_names[] = {"wave"};
    const char* output_names[] = {"prob"};
    OrtValue* output_tensor = NULL;

    status = api->Run(ort_session, NULL, input_names, (const OrtValue* const*)&input_tensor_ort, 1,
                      output_names, 1, &output_tensor);
    if (status != NULL) {
        LOG_ERROR("ONNX Run failed: %s", api->GetErrorMessage(status));
        api->ReleaseStatus(status);
        api->ReleaseValue(input_tensor_ort);
        goto cleanup_heap;
    }

    float* output = NULL;
    status = api->GetTensorMutableData(output_tensor, (void**)&output);
    if (status != NULL) {
        LOG_ERROR("GetTensorMutableData failed: %s", api->GetErrorMessage(status));
        api->ReleaseStatus(status);
        api->ReleaseValue(input_tensor_ort);
        api->ReleaseValue(output_tensor);
        goto cleanup_heap;
    }

    /* Cari pick index (sama logika seperti sebelumnya) */
    int pickIdx = -1;
    float maxP = 0.0f;
    for (int i = 0; i < phasenet_samples; ++i) {
        float probP = output[0 * phasenet_samples + i];
        float probN = output[2 * phasenet_samples + i];
        if (probN < PHASENET_TC && probP > maxP) {
            maxP = probP;
            pickIdx = i;
        }
    }

    if (pickIdx >= 0) {
        double pickTime = minLastTime - WW + (pickIdx * (WW / phasenet_samples));
        result.pickTime = pickTime;
        result.confidence = maxP;
    } else {
        result.pickTime = 0.0;
        result.confidence = 0.0f;
    }

    /* Release ONNX values */
    api->ReleaseValue(input_tensor_ort);
    api->ReleaseValue(output_tensor);

cleanup_heap:
    /* free heap buffers */
    free(downsampled);
    free(normed);
    free(input_tensor);

    return result;
}