#pragma once
#include "esp_err.h"

// Khoi tao camera GC2145: RGB565, QVGA 320x240, 2 frame buffer trong PSRAM
esp_err_t camera_init();
