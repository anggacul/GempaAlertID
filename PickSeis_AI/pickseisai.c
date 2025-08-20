#define PICKSEISAI_VERSION "0.1.0"

#include <stdio.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <omp.h>
#include <time.h>
#include <sys/mman.h>
#include <semaphore.h>
#include "core/config.h"
#include "core/station_manager.h"
#include "core/picking_engine.h"
#include "core/data_window.h"
#include "core/amplitude_extractor.h"
#include "utils/interpolation.h"
#include "utils/logger.h"
#include "utils/sqlite_writer.h"
#include "seedlink_client.h"

volatile int keepRunning = 1;


void handle_signal(int sig) {
    (void)sig;
    keepRunning = 0;
    munmap(ptr, SHM_SIZE);
    close(shm_fd);
    sem_close(sem);
    sem_unlink(SEM_NAME);
    shm_unlink(SHM_NAME);
    cleanupONNXRuntime();
    cleanupSeedLink();
    config_cleanup();
    sqlite_close();
    exit(0);
}

int main(int argc, char* argv[]) {
    // config_init();
    // LOG_INFO("Gagal membaca daftar station dari file %s", STATION_LIST_FILE);


    if (set_sharedmem() != 0) {
        LOG_ERROR("Gagal setting shared memory");
        return 1;
    }
    
    if (!config_load_from_file("config.ini")) {
        LOG_ERROR("Gagal membaca config.ini, gunakan default.");
    }
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // Inisialisasi daftar station dari file
    Station stationList[MAX_STATIONS];

    int stationCount = loadStationListFromFile(STATION_LIST_FILE, stationList, MAX_STATIONS);
    if (stationCount <= 0) {
        LOG_ERROR("Gagal membaca daftar station dari file %s", STATION_LIST_FILE);
        // LOG_ERROR("Gagal membaca daftar station dari file %s", MAX_STATIONS);
        config_cleanup();
        return 1;
    }

    // Inisialisasi SeedLink listener
    if (!startSeedLinkListener(SEEDLINK_HOST, stationList, stationCount, STATEFILE)) {
        LOG_ERROR("Gagal koneksi ke SeedLink server");
        return 1;
    }

    // Inisialisasi ONNX Runtime
    if (!initONNXRuntime()) {
        LOG_ERROR("Gagal inisialisasi ONNX Runtime %s", PHASENET_ONNX_PATH);
        return 1;
    }

    // Inisialisasi SQLite
    if (!sqlite_init("picks.db")) {
        LOG_ERROR("Gagal inisialisasi SQLite");
        return 1;
    }

    // Inisialisasi MQTT, SQLite, dsb.
    // TODO: Implementasi inisialisasi
    LOG_INFO("Station count: %d", stationCount);
    PickState pickStates[MAX_STATIONS] = {0};
    // int num_threads = omp_get_max_threads();
    int num_threads = 4;
    if (stationCount >= 400) {
        #pragma omp parallel num_threads(num_threads)
        {
            int tid = omp_get_thread_num();
            while (keepRunning) {
                for (int i = tid; i < stationCount; i += num_threads) {
                    processStation(&stationList[i], &pickStates[i]);
                }
                usleep(100000); // 100 ms sleep untuk menghindari busy-wait
            }
        }
    } else {
        while (keepRunning) {
            // struct timespec start, end;
            // clock_gettime(CLOCK_MONOTONIC, &start);  // Waktu mulai
            for (int i = 0; i < stationCount; ++i) {
                // processStation(&stationList[i], &pickStates[i]);
                processStation(&stationList[i], &pickStates[i]);  // Proses stasiun

            }
            // clock_gettime(CLOCK_MONOTONIC, &end);  // Waktu selesai

            // Hitung durasi (dalam milidetik)
            // double duration_ms = (end.tv_sec - start.tv_sec) * 1000.0 + 
            //                         (end.tv_nsec - start.tv_nsec) / 1000000.0;

            // printf("[Stasiun Number %d] Waktu proses: %.3f ms\n", stationCount, duration_ms);
            usleep(500000); // 100 ms sleep untuk menghindari busy-wait

        }
    }

    cleanupONNXRuntime();
    cleanupSeedLink();
    munmap(ptr, SHM_SIZE);
    close(shm_fd);
    sem_close(sem);
    sem_unlink(SEM_NAME);
    shm_unlink(SHM_NAME);
    config_cleanup();
    sqlite_close();
    // TODO: Cleanup resource lain jika ada
    return 0;
}
