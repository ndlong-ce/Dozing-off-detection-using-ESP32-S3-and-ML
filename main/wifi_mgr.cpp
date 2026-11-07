#include "wifi_mgr.h"

#include <string.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "config.h"

static const char *TAG = "eye";

static EventGroupHandle_t s_wifi_evt;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
static int s_retry = 0;

static void wifi_event_handler(void *, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry < 10) {
            s_retry++;
            ESP_LOGW(TAG, "WiFi mat ket noi, thu lai %d/10", s_retry);
            esp_wifi_connect();
        } else {
            xEventGroupSetBits(s_wifi_evt, WIFI_FAIL_BIT);
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "=== Mo trinh duyet: http://" IPSTR " ===", IP2STR(&e->ip_info.ip));
        s_retry = 0;
        xEventGroupSetBits(s_wifi_evt, WIFI_CONNECTED_BIT);
    }
}

void wifi_start()
{
    s_wifi_evt = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, nullptr, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, nullptr, nullptr));

    wifi_config_t sta = {};
    strncpy((char *)sta.sta.ssid, WIFI_SSID, sizeof(sta.sta.ssid) - 1);
    strncpy((char *)sta.sta.password, WIFI_PASS, sizeof(sta.sta.password) - 1);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta));
    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(s_wifi_evt, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE, pdFALSE, portMAX_DELAY);
    if (!(bits & WIFI_CONNECTED_BIT)) {
        ESP_LOGW(TAG, "Khong vao duoc WiFi '%s' -> chuyen sang phat WiFi '%s'", WIFI_SSID, AP_SSID);
        s_retry = 100;                       // chan vong thu lai
        esp_wifi_stop();
        wifi_config_t ap = {};
        strncpy((char *)ap.ap.ssid, AP_SSID, sizeof(ap.ap.ssid) - 1);
        strncpy((char *)ap.ap.password, AP_PASS, sizeof(ap.ap.password) - 1);
        ap.ap.ssid_len = strlen(AP_SSID);
        ap.ap.channel = 1;
        ap.ap.max_connection = 4;
        ap.ap.authmode = WIFI_AUTH_WPA2_PSK;
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));
        ESP_ERROR_CHECK(esp_wifi_start());
        ESP_LOGI(TAG, "=== Ket noi WiFi '%s' roi mo: http://192.168.4.1 ===", AP_SSID);
    }
    esp_wifi_set_ps(WIFI_PS_NONE);           // giam tre khi stream
}
