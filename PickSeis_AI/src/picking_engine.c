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
    int phasenet_samples = (int)(50.0 * WW) + 1; // untuk input model (misal 3001)
    float downsampled[3][phasenet_samples];
    double minLastTime = window->minLastTime;
    for (int ch = 0; ch < 3; ++ch) {
        int windowSamples = window->windowSamples[ch];
        double sampleRate = station->sampleRate;
        int pickWindowSamples = (int)(sampleRate * WW) + (int)(sampleRate / 50.0);
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
        float temp[pickWindowSamples];
        int n = 0;
        for (int i = startIdx; i <= endIdx && n < pickWindowSamples; ++i, ++n) {
            temp[n] = window->data[ch][i];
        }
        if (station->sampleRate == 50.0) {
            memcpy(downsampled[ch], temp, sizeof(float) * n);
        } else {
            downsample_to_20hz(temp, n, downsampled[ch], phasenet_samples, sampleRate, 50.0);
        }
        // LOG_INFO("Station %s, ch=%d, timestamp=%.3f, minLastTime=%.3f, startTime=%.3f (idx=%d), endTime=%.3f (idx=%d)", station->stationId, ch, window->timestamp, minLastTime, window->startTime[ch], startIdx, (minLastTime - window->startTime[ch]), endIdx);
    }

    // Normalisasi data 3 komponen terhadap max
    float maxval = 0.0f;
    for (int ch = 0; ch < 3; ++ch) {
        for (int i = 0; i < phasenet_samples; ++i) {
            float v = fabsf(downsampled[ch][i]);
            if (v > maxval) maxval = v;
        }
    }
    if (maxval < 1e-6f) maxval = 1.0f;
    float normed[3][phasenet_samples];
    for (int ch = 0; ch < 3; ++ch) {
        for (int i = 0; i < phasenet_samples; ++i) {
            normed[ch][i] = downsampled[ch][i] / maxval;
        }
    }
    // Siapkan input tensor [1, 3, phasenet_samples]
    float input_tensor[1][3][phasenet_samples];
    for (int ch = 0; ch < 3; ++ch)
        for (int i = 0; i < phasenet_samples; ++i)
            input_tensor[0][ch][i] = normed[ch][i];
    // ONNX inferensi
    const OrtApi* api = OrtGetApiBase()->GetApi(ORT_API_VERSION);
    OrtMemoryInfo* mem_info;
    api->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault, &mem_info);
    int64_t input_shape[3] = {1, 3, phasenet_samples};
    OrtValue* input_tensor_ort = NULL;
    api->CreateTensorWithDataAsOrtValue(mem_info, input_tensor, sizeof(input_tensor), input_shape, 3, ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &input_tensor_ort);
    api->ReleaseMemoryInfo(mem_info);
    // Nama input/output sesuai model PhaseNet
    const char* input_names[] = {"wave"};
    const char* output_names[] = {"prob"};
    OrtValue* output_tensor = NULL;
    OrtStatus* status = api->Run(ort_session, NULL, input_names, (const OrtValue* const*)&input_tensor_ort, 1, output_names, 1, &output_tensor);
    // Mendapatkan informasi tipe dan bentuk tensor output
    // OrtTensorTypeAndShapeInfo* info;
    // api->GetTensorTypeAndShape(output_tensor, &info);

    // // Mengambil jumlah dimensi tensor
    // size_t dim_count;
    // api->GetDimensionsCount(info, &dim_count);

    // // Mengambil dimensi tensor
    // int64_t dims[dim_count];
    // api->GetDimensions(info, dims, dim_count);

    // // Cetak ukuran tensor output
    // printf("Output Tensor Dimensi: [");
    // for (size_t i = 0; i < dim_count; ++i) {
    //     printf("%lld", dims[i]);
    //     if (i < dim_count - 1) {
    //         printf(", ");
    //     }
    // }
    // printf("]\n");

    // // Melepaskan info tipe dan bentuk
    // api->ReleaseTensorTypeAndShapeInfo(info);
    // Output: [1, 600, 3] (P, S, N)
    float* output = NULL;
    api->GetTensorMutableData(output_tensor, (void**)&output);
    // Cari waktu pick (max output class P)
    int pickIdx = -1;
    float maxP = 0.0f;
    float minN = 0.0f;
    for (int i = 0; i < phasenet_samples; ++i) {
        float probP = output[0 * phasenet_samples + i];  // channel 0 = P
        float probN = output[2 * phasenet_samples + i];  // channel 2 = N
        // if (probN < PHASENET_TC) {
        //     minN = probP;
        //     LOG_INFO("maxP %f %s", minN, station->stationId);
        // }
        if (probN < PHASENET_TC && probP > maxP) {
            maxP = probP;
            pickIdx = i;
        }
    }
    
    if (pickIdx >= 0) {
        // Waktu pick relatif terhadap start window sinkron
        double pickTime = minLastTime - WW + (pickIdx * (WW / phasenet_samples));
        result.pickTime = pickTime;
        result.confidence = maxP;
    } else {
        result.pickTime = 0.0;
        result.confidence = 0.0f;
    }
    api->ReleaseValue(input_tensor_ort);
    api->ReleaseValue(output_tensor);
    return result;
} 