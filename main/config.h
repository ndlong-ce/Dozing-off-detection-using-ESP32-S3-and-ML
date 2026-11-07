#pragma once
// =====================================================================
//  CAU HINH CHUNG - sua cac gia tri o day (WiFi, chan GPIO, nguong...)
// =====================================================================
#include "driver/gpio.h"

// ---- WiFi ----
#define WIFI_SSID       "Kayungg"      // <-- SUA
#define WIFI_PASS       "dlongdepzai"         // <-- SUA
#define AP_SSID         "EyeCam-ESP32"          // neu khong vao duoc WiFi -> tu phat WiFi nay
#define AP_PASS         "12345678"              // (>= 8 ky tu) IP se la 192.168.4.1

// ---- LED / Buzzer ----
#define LED_GPIO        GPIO_NUM_2
#define LED_ON_LEVEL    1        // doi thanh 0 neu LED tren board sang muc thap
#define BUZZER_GPIO     GPIO_NUM_1
#define BUZZER_ON_LEVEL 0        // module 3 chan (I/O,VCC,GND) thuong kich MUC THAP: 0 = keu khi I/O=LOW. Neu keu lien tuc luc binh thuong -> doi thanh 1
#define BUZZER_ACTIVE   1        // 1: buzzer chu dong (chi bat/tat, cao do co dinh theo phan cung); 0: buzzer thu dong -> phat dung BUZZER_FREQ_HZ bang PWM
#define BUZZER_DELAY_MS 500     // LED sang lien tuc >= ngan nay (ms) thi buzzer moi keu
#define BUZZER_FREQ_HZ  1000     // tan so tieng keu (Hz) - chi co tac dung voi buzzer thu dong (BUZZER_ACTIVE 0)

// ---- Nhan dien mat nham ----
#define CLOSED_FRAMES   3        // so khung lien tiep nham moi bao dong
#define EYE_SIDE_RATIO  0.60f    // canh o crop = ty le * khoang cach 2 mat. CHINH SO NAY neu sai
#define IMG_SIZE        32
#define CLOSED_THRESH   0.15f    // P(closed) > nguong nay thi tinh la nham
#define JPEG_QUALITY    60       // 1..100 (cao = net hon, cham hon)

// ---- Pinout ESP32-S3-CAM (khop so do ban gui) ----
#define CAM_XCLK 15
#define CAM_SIOD 4
#define CAM_SIOC 5
#define CAM_VSYNC 6
#define CAM_HREF 7
#define CAM_PCLK 13
#define CAM_Y2 11
#define CAM_Y3 9
#define CAM_Y4 8
#define CAM_Y5 10
#define CAM_Y6 12
#define CAM_Y7 18
#define CAM_Y8 17
#define CAM_Y9 16

#define BUZZER_SELFTEST 1
