#include "led.h"

#include "driver/gpio.h"
#include "config.h"

void led_init()
{
    gpio_config_t io = {};
    io.pin_bit_mask = 1ULL << LED_GPIO;
    io.mode = GPIO_MODE_OUTPUT;
    gpio_config(&io);
    gpio_set_level(LED_GPIO, !LED_ON_LEVEL);
}

void led_set(bool on)
{
    gpio_set_level(LED_GPIO, on ? LED_ON_LEVEL : !LED_ON_LEVEL);
}
