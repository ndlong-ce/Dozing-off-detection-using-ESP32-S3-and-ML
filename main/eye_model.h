#pragma once
// CNN mat mo/nham (EyeNet INT8, ESP-DL): crop 32x32 xam quanh mat -> xac suat "nham"
#include <stdint.h>
#include "config.h"
#include "dl_model_base.hpp"

struct EyeOut {
    float p_closed = 0.0f;                 // xac suat nham (class 0)
    float l0 = 0.0f, l1 = 0.0f;            // logits (0 = nham, 1 = mo)
    uint8_t crop[IMG_SIZE * IMG_SIZE] = {};   // o 32x32 xam ma CNN nhin thay
};

class EyeClassifier {
public:
    void init();   // nap model nhung san trong firmware (eye_cnn.espdl)
    // buf: khung RGB565 (byte cao truoc), (cx,cy): tam mat, side: canh o crop (pixel)
    void predict(const uint8_t *buf, int w, int h, float cx, float cy, float side, EyeOut &out);

private:
    dl::Model *model_ = nullptr;
    dl::TensorBase *in_ = nullptr;
    dl::TensorBase *out_ = nullptr;
};
