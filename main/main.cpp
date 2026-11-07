// ESP32-S3 CAM - giam sat ngu gat:
//   camera -> tim mat (ESP-DL) -> CNN mat mo/nham -> LED GPIO2 + buzzer GPIO1
//   + WiFi + web (xem camera truc tiep, trang thai mat)
//
// Cac module (moi file lam 1 viec):
//   config.h        cau hinh chung (WiFi, GPIO, nguong)
//   camera.*        khoi tao camera GC2145
//   eye_model.*     CNN mat mo/nham (INT8, ESP-DL)
//   eye_monitor.*   tim mat + chay CNN cho 2 mat moi khung hinh
//   alarm_logic.*   dem khung nham, hen gio buzzer
//   led.*, buzzer.* dau ra canh bao
//   overlay.*       ve khung len anh
//   shared.*        du lieu dung chung voi web server
//   wifi_mgr.*, web_server.*, web_page.h   WiFi + web
#include <stdlib.h>
#include "esp_camera.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "img_converters.h"
#include "nvs_flash.h"

#include "alarm_logic.h"
#include "buzzer.h"
#include "camera.h"
#include "config.h"
#include "eye_monitor.h"
#include "led.h"
#include "overlay.h"
#include "shared.h"
#include "web_server.h"
#include "wifi_mgr.h"

static const char *TAG = "eye";

extern "C" void app_main(void)
{
    led_init();
    buzzer_init();   // goi som de buzzer im lang ngay tu dau

    // NVS (WiFi can)
    esp_err_t r = nvs_flash_init();
    if (r == ESP_ERR_NVS_NO_FREE_PAGES || r == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        r = nvs_flash_init();
    }
    ESP_ERROR_CHECK(r);

    shared_init();
    ESP_ERROR_CHECK(camera_init());

    EyeMonitor monitor;
    monitor.init();

    wifi_start();
    web_start();

    AlarmLogic alarm_logic;
    float fps = 0.0f;
    int64_t t_prev = esp_timer_get_time();

    while (true) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) { vTaskDelay(pdMS_TO_TICKS(10)); continue; }

        // 1) nhan dien
        EyeFrameResult res = monitor.analyze(fb);

        // 2) canh bao: LED sang khi mat nham du lau; LED sang lien tuc >= BUZZER_DELAY_MS thi buzzer keu
        AlarmState st = alarm_logic.update(res.face, res.p_closed, esp_timer_get_time());
        led_set(st.alarm);
        buzzer_request(st.buzz);

        // 3) ve khung, nen JPEG va day cho web
        overlay_draw(fb->buf, fb->width, fb->height, res, st.alarm);
        uint8_t *jpg = nullptr;
        size_t jlen = 0;
        if (fmt2jpg(fb->buf, fb->len, fb->width, fb->height, PIXFORMAT_RGB565, JPEG_QUALITY, &jpg, &jlen)) {
            publish_frame(jpg, jlen);
            free(jpg);
        }

        int64_t now = esp_timer_get_time();
        float inst = 1e6f / (float)(now - t_prev);
        t_prev = now;
        fps = (fps == 0.0f) ? inst : (0.8f * fps + 0.2f * inst);
        publish_status(res.face, res.p_closed, st.closed_cnt, st.alarm, fps);

        ESP_LOGI(TAG, "face=%d  P(closed)=%.2f  cnt=%d  LED=%d  ALM=%dms  BUZ=%d  fps=%.1f",
                 res.face, res.p_closed, st.closed_cnt, st.alarm, st.alarm_ms, st.buzz, fps);
        esp_camera_fb_return(fb);
        vTaskDelay(1);   // nhuong CPU cho IDLE0 (tranh task watchdog)
    }
}
