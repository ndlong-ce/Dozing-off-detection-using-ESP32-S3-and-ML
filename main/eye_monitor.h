#pragma once
// Moi khung hinh: tim mat (ESP-DL human_face_detect) -> crop 2 mat -> CNN -> trung binh P(closed)
#include "esp_camera.h"
#include "eye_model.h"
#include "human_face_detect.hpp"

struct EyeFrameResult {
    bool face = false;          // true neu thay mat va CNN da chay
    float p_closed = 0.0f;      // trung binh xac suat nham cua 2 mat
    float lx = 0, ly = 0, rx = 0, ry = 0;   // tam mat trai / phai (pixel)
    float side = 0;             // canh o crop quanh moi mat
    int bx0 = 0, by0 = 0, bx1 = 0, by1 = 0; // khung mat
    bool have_box = false;
};

class EyeMonitor {
public:
    void init();                               // nap CNN + bo do mat
    EyeFrameResult analyze(camera_fb_t *fb);   // fb: khung RGB565 tu esp_camera_fb_get()

private:
    EyeClassifier eye_;
    HumanFaceDetect *detector_ = nullptr;
};
