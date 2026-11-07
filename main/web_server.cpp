#include "web_server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "buzzer.h"
#include "config.h"
#include "shared.h"
#include "web_page.h"

static const char *TAG = "eye";

static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t status_handler(httpd_req_t *req)
{
    char buf[160];
    xSemaphoreTake(g_state.mtx, portMAX_DELAY);
    int n = snprintf(buf, sizeof(buf),
                     "{\"face\":%d,\"p_closed\":%.3f,\"cnt\":%d,\"alarm\":%d,\"fps\":%.2f,\"buzz\":%d}",
                     g_state.face ? 1 : 0, g_state.p_closed, g_state.cnt, g_state.alarm ? 1 : 0, g_state.fps, buzzer_requested() ? 1 : 0);
    xSemaphoreGive(g_state.mtx);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, buf, n);
}

// Anh BMP 8-bit 64x32: o mat trai | o mat phai ma CNN nhin thay (de debug)
static inline void put_le32(uint8_t *p, uint32_t v)
{
    p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF; p[2] = (v >> 16) & 0xFF; p[3] = (v >> 24) & 0xFF;
}

static esp_err_t crops_handler(httpd_req_t *req)
{
    const int W = IMG_SIZE * 2, H = IMG_SIZE;
    const int pal = 256 * 4;
    const int off = 14 + 40 + pal;
    const int total = off + W * H;
    uint8_t bmp[14 + 40 + 256 * 4 + IMG_SIZE * 2 * IMG_SIZE];
    memset(bmp, 0, sizeof(bmp));
    bmp[0] = 'B'; bmp[1] = 'M';
    put_le32(bmp + 2, total);
    put_le32(bmp + 10, off);
    put_le32(bmp + 14, 40);
    put_le32(bmp + 18, W);
    put_le32(bmp + 22, H);
    bmp[26] = 1;                 // planes
    bmp[28] = 8;                 // bpp
    put_le32(bmp + 34, W * H);
    put_le32(bmp + 46, 256);
    for (int i = 0; i < 256; i++) {          // bang mau xam (B,G,R,0)
        bmp[54 + i * 4 + 0] = i; bmp[54 + i * 4 + 1] = i; bmp[54 + i * 4 + 2] = i;
    }
    xSemaphoreTake(g_state.mtx, portMAX_DELAY);
    for (int y = 0; y < H; y++) {            // BMP luu tu duoi len
        uint8_t *row = bmp + off + (H - 1 - y) * W;
        memcpy(row, &g_state.crop[0][y * IMG_SIZE], IMG_SIZE);
        memcpy(row + IMG_SIZE, &g_state.crop[1][y * IMG_SIZE], IMG_SIZE);
    }
    xSemaphoreGive(g_state.mtx);
    httpd_resp_set_type(req, "image/bmp");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, (const char *)bmp, total);
}

// Mo http://<IP>/beep de buzzer keu thu 1,5 giay (khong can nhan dien mat)
static esp_err_t beep_handler(httpd_req_t *req)
{
    buzzer_test(1500);
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, "beep 1.5s", HTTPD_RESP_USE_STRLEN);
}

static esp_err_t stream_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=frame");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    uint8_t *local = nullptr;
    size_t lcap = 0;
    uint32_t last = 0;
    esp_err_t res = ESP_OK;

    while (res == ESP_OK) {
        size_t len = 0;
        bool oom = false;

        xSemaphoreTake(g_state.mtx, portMAX_DELAY);
        if (g_state.len && g_state.seq != last) {
            if (g_state.len > lcap) {
                free(local);
                lcap = g_state.len + 4096;
                local = (uint8_t *)heap_caps_malloc(lcap, MALLOC_CAP_SPIRAM);
                if (!local) { lcap = 0; oom = true; }
            }
            if (!oom) {
                memcpy(local, g_state.jpg, g_state.len);
                len = g_state.len;
                last = g_state.seq;
            }
        }
        xSemaphoreGive(g_state.mtx);

        if (oom) { res = ESP_FAIL; break; }
        if (!len) { vTaskDelay(pdMS_TO_TICKS(20)); continue; }

        char hdr[96];
        int n = snprintf(hdr, sizeof(hdr),
                         "\r\n--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
                         (unsigned)len);
        res = httpd_resp_send_chunk(req, hdr, n);
        if (res == ESP_OK) res = httpd_resp_send_chunk(req, (const char *)local, len);
    }
    free(local);
    return res;
}

void web_start()
{
    // Server 1: cong 80
    httpd_config_t c1 = HTTPD_DEFAULT_CONFIG();
    c1.server_port = 80;
    c1.stack_size = 8192;
    c1.max_open_sockets = 5;
    c1.lru_purge_enable = true;
    httpd_handle_t h1 = nullptr;
    if (httpd_start(&h1, &c1) == ESP_OK) {
        httpd_uri_t idx = {}; idx.uri = "/";       idx.method = HTTP_GET; idx.handler = index_handler;
        httpd_uri_t st  = {}; st.uri  = "/status"; st.method  = HTTP_GET; st.handler  = status_handler;
        httpd_register_uri_handler(h1, &idx);
        httpd_register_uri_handler(h1, &st);
        httpd_uri_t cr = {}; cr.uri = "/crops"; cr.method = HTTP_GET; cr.handler = crops_handler;
        httpd_register_uri_handler(h1, &cr);
        httpd_uri_t bp = {}; bp.uri = "/beep"; bp.method = HTTP_GET; bp.handler = beep_handler;
        httpd_register_uri_handler(h1, &bp);
    } else {
        ESP_LOGE(TAG, "Khong start duoc web server cong 80");
    }

    // Server 2: cong 81 (rieng cho stream vi handler stream chiem tron 1 luong)
    httpd_config_t c2 = HTTPD_DEFAULT_CONFIG();
    c2.server_port = 81;
    c2.ctrl_port = c1.ctrl_port + 1;
    c2.stack_size = 8192;
    c2.max_open_sockets = 5;
    c2.lru_purge_enable = true;
    httpd_handle_t h2 = nullptr;
    if (httpd_start(&h2, &c2) == ESP_OK) {
        httpd_uri_t sm = {}; sm.uri = "/stream"; sm.method = HTTP_GET; sm.handler = stream_handler;
        httpd_register_uri_handler(h2, &sm);
    } else {
        ESP_LOGE(TAG, "Khong start duoc web server cong 81");
    }
}
