#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/config.h"
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

char* STATION_LIST_FILE;

double WW;
double WSHIFT;
int SAMPLES_PER_WINDOW;

double FREQMIN;
double FREQMAX;

double PICK_TT;
int PICK_NT;
double MAX_GAP;

char* PHASENET_ONNX_PATH;
char* SEEDLINK_HOST;
char* STATEFILE;
double PHASENET_TC;// Threshold probability N

int shm_fd;
int current_counter = 0;
sem_t *sem;
shared_data *ptr;

static void set_string(char **dest, const char *src) {
    if (*dest) {
        free(*dest);
        *dest = NULL;
    }
    if (src) {
        *dest = strdup(src);
    }
}

int set_sharedmem() {
    // 1. Buat semafor
    sem = sem_open(SEM_NAME, O_CREAT, 0666, 0);
    if (sem == SEM_FAILED) {
        perror("sem_open failed");
        return -1;
    }

    // 2. Buat shared memory
    // Gunakan shm_fd global secara langsung
    shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open failed");
        return -1;
    }
    ftruncate(shm_fd, SHM_SIZE);

    // 3. Map shared memory ke pointer shared_data
    ptr = mmap(NULL, SHM_SIZE, PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (ptr == MAP_FAILED) {
        perror("mmap failed");
        return -1;
    }

    return 0;
}

void config_init() {
    STATION_LIST_FILE =  strdup("station_list.txt");
    WW = 10.0;
    WSHIFT = 1.0;
    SAMPLES_PER_WINDOW = 6000;
    FREQMIN = 1.0;
    FREQMAX = 20.0;
    PICK_TT = 3.0;
    PICK_NT = 5;
    MAX_GAP = 2.0;
    PHASENET_TC = 0.5;
    set_string(&PHASENET_ONNX_PATH, "phasenet.onnx");
    set_string(&SEEDLINK_HOST, "202.90.199.206:18000");
    set_string(&STATEFILE, "statefile.state");
}

void config_cleanup() {
    if (STATION_LIST_FILE) { free(STATION_LIST_FILE); STATION_LIST_FILE = NULL; }
    if (PHASENET_ONNX_PATH) { free(PHASENET_ONNX_PATH); PHASENET_ONNX_PATH = NULL; }
    if (SEEDLINK_HOST) { free(SEEDLINK_HOST); SEEDLINK_HOST = NULL; }
}

int config_load_from_file(const char* filename) {
    FILE* f = fopen(filename, "r");
    if (!f) return 0;
    char line[256], section[32] = "";
    while (fgets(line, sizeof(line), f)) {
        char* s = line;
        while (*s == ' ' || *s == '\t') ++s;
        if (*s == '#' || *s == ';' || *s == '\n' || *s == '\0') continue;
        if (*s == '[') {
            char* e = strchr(s, ']');
            if (e) {
                size_t len = e - (s+1);
                if (len < sizeof(section)) {
                    strncpy(section, s+1, len);
                    section[len] = '\0';
                }
            }
            continue;
        }
        char* eq = strchr(s, '=');
        if (eq) {
            *eq = 0;
            char* key = s;
            char* val = eq + 1;
            // fprintf(stdout, "print isi config file %s\n", val);
            // Trim leading/trailing space key
            while (*key == ' ' || *key == '\t') ++key;
            char* keyend = key + strlen(key) - 1;
            while (keyend > key && (*keyend == ' ' || *keyend == '\t')) *keyend-- = 0;
            // Trim leading/trailing space val
            while (*val == ' ' || *val == '\t') ++val;
            char* valend = val + strlen(val) - 1;
            while (valend > val && (*valend == ' ' || *valend == '\t' || *valend == '\n' || *valend == '\r')) *valend-- = 0;
            *(valend+1) = 0;
            // General
            if (strcmp(section, "general") == 0) {
                if (strcmp(key, "station_list_file") == 0) set_string(&STATION_LIST_FILE, val);
                else if (strcmp(key, "phasenet_onnx_path") == 0) set_string(&PHASENET_ONNX_PATH, val);
                else if (strcmp(key, "statefile") == 0) set_string(&STATEFILE, val);
            } else if (strcmp(section, "window") == 0) {
                if (strcmp(key, "ww") == 0) WW = atof(val);
                else if (strcmp(key, "wshift") == 0) WSHIFT = atof(val);
                else if (strcmp(key, "samples_per_window") == 0) SAMPLES_PER_WINDOW = atoi(val);
            } else if (strcmp(section, "bandpass") == 0) {
                if (strcmp(key, "freqmin") == 0) FREQMIN = atof(val);
                else if (strcmp(key, "freqmax") == 0) FREQMAX = atof(val);
            } else if (strcmp(section, "picking") == 0) {
                if (strcmp(key, "pick_tt") == 0) PICK_TT = atof(val);
                else if (strcmp(key, "pick_nt") == 0) PICK_NT = atoi(val);
                else if (strcmp(key, "max_gap") == 0) MAX_GAP = atof(val);
                else if (strcmp(key, "phasenet_tc") == 0) PHASENET_TC = atof(val);
            } else if (strcmp(section, "seedlink") == 0) {
                if (strcmp(key, "host") == 0) set_string(&SEEDLINK_HOST, val);
            }
        }
    }
    fclose(f);
    return 1;
} 
