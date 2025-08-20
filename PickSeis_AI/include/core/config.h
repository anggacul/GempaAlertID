#pragma once

// Ukuran buffer dan metadata
#define MAX_CHANNELS 3
#define MAX_STATION_ID_LEN 6
#define MAX_CHANNEL_NAME_LEN 4
#define MAX_STATIONS 500

// File konfigurasi
extern char* STATION_LIST_FILE;

// Window dan sample
extern double WW;
extern double WSHIFT;
extern int SAMPLES_PER_WINDOW;

// Bandpass filter
extern double FREQMIN;
extern double FREQMAX;

// Picking
extern double PICK_TT;
extern int PICK_NT;
extern double MAX_GAP;
extern double PHASENET_TC;

// Model ONNX
extern char* PHASENET_ONNX_PATH;

// Seedlink host
extern char* SEEDLINK_HOST;

// State file
extern char* STATEFILE;

typedef struct {
    int counter;
    char data[SHM_SIZE - sizeof(int)];
} shared_data;

void config_init();
int config_load_from_file(const char* filename);
void config_cleanup(); 
