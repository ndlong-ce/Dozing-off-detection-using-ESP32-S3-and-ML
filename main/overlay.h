#pragma once
// Ve khung len anh RGB565 de xem tren web
#include <stdint.h>
#include "eye_monitor.h"

// Khung vang quanh mat, 2 o vuong o hai mat (xanh = mo, do = nham), vien do khi bao dong
void overlay_draw(uint8_t *buf, int w, int h, const EyeFrameResult &r, bool alarm);
