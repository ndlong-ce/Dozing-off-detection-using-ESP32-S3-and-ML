#include "camera.h"

#include "esp_camera.h"
#include "config.h"

esp_err_t camera_init()
{
    camera_config_t c = {};
    c.ledc_channel = LEDC_CHANNEL_0;
    c.ledc_timer = LEDC_TIMER_0;
    c.pin_d0 = CAM_Y2; c.pin_d1 = CAM_Y3; c.pin_d2 = CAM_Y4; c.pin_d3 = CAM_Y5;
    c.pin_d4 = CAM_Y6; c.pin_d5 = CAM_Y7; c.pin_d6 = CAM_Y8; c.pin_d7 = CAM_Y9;
    c.pin_xclk = CAM_XCLK; c.pin_pclk = CAM_PCLK;
    c.pin_vsync = CAM_VSYNC; c.pin_href = CAM_HREF;
    c.pin_sccb_sda = CAM_SIOD; c.pin_sccb_scl = CAM_SIOC;
    c.pin_pwdn = -1; c.pin_reset = -1;
    c.xclk_freq_hz = 20000000;
    c.pixel_format = PIXFORMAT_RGB565;
    c.frame_size = FRAMESIZE_QVGA;   // 320x240
    c.fb_count = 2;
    c.fb_location = CAMERA_FB_IN_PSRAM;
    c.grab_mode = CAMERA_GRAB_LATEST;
    return esp_camera_init(&c);
}
