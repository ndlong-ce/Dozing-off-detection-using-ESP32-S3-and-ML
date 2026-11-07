#pragma once
// Du lieu dung chung giua vong AI (main) va web server
#include <stddef.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "config.h"

struct Shared {
    SemaphoreHandle_t mtx = nullptr;
    uint8_t *jpg = nullptr;   // anh JPEG moi nhat (PSRAM)
    size_t cap = 0;
    size_t len = 0;
    uint32_t seq = 0;
    bool face = false;
    float p_closed = 0.0f;
    int cnt = 0;
    bool alarm = false;
    float fps = 0.0f;
    // debug: o mat 32x32 (xam) va logits ma CNN nhin thay
    uint8_t crop[2][IMG_SIZE * IMG_SIZE] = {};
    float logit[2][2] = {};
};
extern Shared g_state;

void shared_init();                                           // tao mutex (goi 1 lan truoc khi dung)
void publish_frame(const uint8_t *jpg, size_t len);           // anh JPEG cho /stream
void publish_status(bool face, float p_closed, int cnt, bool alarm, float fps);   // cho /status
void publish_eye_debug(int eye, const uint8_t *crop, float l0, float l1);         // cho /crops
