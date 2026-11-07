#include "shared.h"

#include <string.h>
#include "esp_heap_caps.h"

Shared g_state;

void shared_init()
{
    g_state.mtx = xSemaphoreCreateMutex();
}

void publish_frame(const uint8_t *jpg, size_t len)
{
    xSemaphoreTake(g_state.mtx, portMAX_DELAY);
    if (len > g_state.cap) {
        uint8_t *nb = (uint8_t *)heap_caps_realloc(g_state.jpg, len + 4096, MALLOC_CAP_SPIRAM);
        if (nb) { g_state.jpg = nb; g_state.cap = len + 4096; }
    }
    if (len <= g_state.cap) {
        memcpy(g_state.jpg, jpg, len);
        g_state.len = len;
        g_state.seq++;
    }
    xSemaphoreGive(g_state.mtx);
}

void publish_status(bool face, float p_closed, int cnt, bool alarm, float fps)
{
    xSemaphoreTake(g_state.mtx, portMAX_DELAY);
    g_state.face = face; g_state.p_closed = p_closed; g_state.cnt = cnt; g_state.alarm = alarm; g_state.fps = fps;
    xSemaphoreGive(g_state.mtx);
}

void publish_eye_debug(int eye, const uint8_t *crop, float l0, float l1)
{
    xSemaphoreTake(g_state.mtx, portMAX_DELAY);
    memcpy(g_state.crop[eye], crop, IMG_SIZE * IMG_SIZE);
    g_state.logit[eye][0] = l0;
    g_state.logit[eye][1] = l1;
    xSemaphoreGive(g_state.mtx);
}
