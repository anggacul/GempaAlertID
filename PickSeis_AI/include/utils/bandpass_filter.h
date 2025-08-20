#pragma once
#include <stddef.h>
void bandpassFilter(float* data, size_t length, double sampleRate, double freqmin, double freqmax); 