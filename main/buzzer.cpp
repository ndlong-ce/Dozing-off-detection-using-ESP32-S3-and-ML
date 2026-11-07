#include "buzzer.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "config.h"

// =====================================================================
//  Buzzer: keu lien tuc khi duoc yeu cau (buzzer_request(true)), tat ngay khi buzzer_request(false)
//  - BUZZER_ACTIVE=1: buzzer chu dong -> chi bat/tat (cao do do phan cung quyet dinh, khong chinh duoc)
//  - BUZZER_ACTIVE=0: buzzer thu dong -> phat dung BUZZER_FREQ_HZ bang PWM (LEDC timer1/channel1)
// =====================================================================
static volatile bool s_req = false;   // true = yeu cau buzzer keu (vong AI dat, task buzzer doc)

static volatile uint32_t s_test_until_ms = 0;   // keu thu cho den thoi diem nay (ms, tinh tu luc khoi dong)

static uint32_t now_ms() { return (uint32_t)(esp_timer_get_time() / 1000); }

void buzzer_request(bool on) { s_req = on; }
bool buzzer_requested() { return s_req; }
void buzzer_test(int ms) { s_test_until_ms = now_ms() + (uint32_t)ms; }

static void buzzer_on(int freq_hz)
{
#if BUZZER_ACTIVE
    (void)freq_hz;
    gpio_set_level(BUZZER_GPIO, BUZZER_ON_LEVEL);
#else
    ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_1, (uint32_t)freq_hz);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 512);   // 50% cua 10 bit
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
#endif
}

static void buzzer_off()
{
#if BUZZER_ACTIVE
    gpio_set_level(BUZZER_GPIO, !BUZZER_ON_LEVEL);
#else
    ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, BUZZER_ON_LEVEL ? 0 : 1);
#endif
}

static void buzzer_task(void *)
{
    bool sounding = false;
    while (true) {
        bool want = s_req || ((int32_t)(s_test_until_ms - now_ms()) > 0);
        if (want && !sounding) {
            buzzer_on(BUZZER_FREQ_HZ);
            sounding = true;
        } else if (!want && sounding) {
            buzzer_off();
            sounding = false;
        }
        TickType_t tk = pdMS_TO_TICKS(10);
        if (tk == 0) tk = 1;
        vTaskDelay(tk);
    }
}

void buzzer_init()
{
    gpio_config_t bz = {};
    bz.pin_bit_mask = 1ULL << BUZZER_GPIO;
    bz.mode = GPIO_MODE_OUTPUT;
    gpio_config(&bz);
    gpio_set_level(BUZZER_GPIO, !BUZZER_ON_LEVEL);   // im lang ngay tu dau
#if !BUZZER_ACTIVE
    ledc_timer_config_t tc = {};
    tc.speed_mode = LEDC_LOW_SPEED_MODE;
    tc.duty_resolution = LEDC_TIMER_10_BIT;
    tc.timer_num = LEDC_TIMER_1;
    tc.freq_hz = BUZZER_FREQ_HZ;
    tc.clk_cfg = LEDC_AUTO_CLK;
    ledc_timer_config(&tc);
    ledc_channel_config_t cc = {};
    cc.gpio_num = BUZZER_GPIO;
    cc.speed_mode = LEDC_LOW_SPEED_MODE;
    cc.channel = LEDC_CHANNEL_1;
    cc.intr_type = LEDC_INTR_DISABLE;
    cc.timer_sel = LEDC_TIMER_1;
    cc.duty = 0;
    cc.hpoint = 0;
    ledc_channel_config(&cc);
    buzzer_off();
#endif
    xTaskCreate(buzzer_task, "buzzer", 3072, nullptr, 3, nullptr);
#if defined(BUZZER_SELFTEST) && BUZZER_SELFTEST
    buzzer_test(400);   // keu ngan luc khoi dong: nghe thay = phan cung va muc kich dung
#endif
}
