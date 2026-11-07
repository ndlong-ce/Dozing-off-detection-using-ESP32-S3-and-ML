#include "eye_model.h"

#include <math.h>
#include <algorithm>
#include "esp_log.h"

static const char *TAG = "eye";

extern const uint8_t eye_model_start[] asm("_binary_eye_cnn_espdl_start");

// Do sang (0..255) tai pixel (x,y) cua khung RGB565 (byte cao truoc)
static inline uint8_t gray_at(const uint8_t *buf, int w, int h, int x, int y)
{
    x = std::max(0, std::min(w - 1, x));
    y = std::max(0, std::min(h - 1, y));
    const uint8_t *p = buf + 2 * (y * w + x);
    uint16_t v = (uint16_t(p[0]) << 8) | p[1];
    int r = ((v >> 11) & 0x1F) << 3, g = ((v >> 5) & 0x3F) << 2, b = (v & 0x1F) << 3;
    return (uint8_t)((77 * r + 150 * g + 29 * b) >> 8);
}

// Crop o vuong tam (cx,cy) canh `side` -> 32x32, chuan hoa (g/255-0.5)/0.5, luong tu hoa INT8 theo exponent cua input
static void crop_to_input(const uint8_t *buf, int w, int h, float cx, float cy, float side,
                          dl::TensorBase *in, uint8_t *gray_out)
{
    int8_t *dst = (int8_t *)in->data;
    const float q = powf(2.0f, (float)-in->exponent);   // int8 = round(x / 2^exp)
    for (int j = 0; j < IMG_SIZE; j++) {
        for (int i = 0; i < IMG_SIZE; i++) {
            float sx = cx + ((i + 0.5f) / IMG_SIZE - 0.5f) * side;
            float sy = cy + ((j + 0.5f) / IMG_SIZE - 0.5f) * side;
            float g = gray_at(buf, w, h, (int)lroundf(sx), (int)lroundf(sy));
            gray_out[j * IMG_SIZE + i] = (uint8_t)g;
            float x = (g / 255.0f - 0.5f) / 0.5f;
            int v = (int)lroundf(x * q);
            dst[j * IMG_SIZE + i] = (int8_t)std::max(-128, std::min(127, v));
        }
    }
}

void EyeClassifier::init()
{
    model_ = new dl::Model((const char *)eye_model_start, fbs::MODEL_LOCATION_IN_FLASH_RODATA);
    in_ = model_->get_inputs().begin()->second;
    out_ = model_->get_outputs().begin()->second;
    ESP_LOGI(TAG, "input exp=%d  output exp=%d", in_->get_exponent(), out_->get_exponent());
}

// Tra ve xac suat "nham" (class 0). Class 1 = mo.
void EyeClassifier::predict(const uint8_t *buf, int w, int h, float cx, float cy, float side, EyeOut &o)
{
    crop_to_input(buf, w, h, cx, cy, side, in_, o.crop);
    model_->run();
    const int8_t *p = (const int8_t *)out_->data;
    const float s = powf(2.0f, (float)out_->exponent);
    o.l0 = p[0] * s;
    o.l1 = p[1] * s;
    float m = std::max(o.l0, o.l1);
    float e0 = expf(o.l0 - m), e1 = expf(o.l1 - m);
    o.p_closed = e0 / (e0 + e1);
}
