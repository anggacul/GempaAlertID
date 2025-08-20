#pragma once
#include <stdio.h>

/**
 * @brief Log pesan info
 */
#define LOG_INFO(fmt, ...) fprintf(stdout, "[INFO] " fmt "\n", ##__VA_ARGS__)

/**
 * @brief Log pesan error
 */
#define LOG_ERROR(fmt, ...) fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__) 