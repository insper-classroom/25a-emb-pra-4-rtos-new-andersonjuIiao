/*
 * LED blink with FreeRTOS
 */
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#include "ssd1306.h"
#include "gfx.h"

#include "pico/stdlib.h"
#include <stdio.h>

#define ECHO_PIN 27
#define TRIG_PIN 28

QueueHandle_t xQueueDistance;
SemaphoreHandle_t xSemaphoreTrigger;

void trigger_task(void *p)
{
    gpio_init(TRIG_PIN);
    gpio_set_dir(TRIG_PIN, GPIO_OUT);
    
    while (true)
    {
        gpio_put(TRIG_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_put(TRIG_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void echo_task(void *p) {
    gpio_init(ECHO_PIN);
    gpio_set_dir(ECHO_PIN, GPIO_IN);
    while (true) {
        while (gpio_get(ECHO_PIN) == 0);
        absolute_time_t start = get_absolute_time();
        while (gpio_get(ECHO_PIN) == 1);
        absolute_time_t end = get_absolute_time();
        
        uint32_t duration = absolute_time_diff_us(start, end);
        float distance = duration * 0.017015;
        xQueueSend(xQueueDistance, &distance, 0);
        xSemaphoreGive(xSemaphoreTrigger);
    }
}

void oled_task(void *p) {
    ssd1306_t tela;
    ssd1306_init();
    gfx_init(&tela, 128, 32);
    
    while (true) {
        if (xSemaphoreTake(xSemaphoreTrigger, pdMS_TO_TICKS(500)) == pdTRUE) {
            float distancia;
            if (xQueueReceive(xQueueDistance, &distancia, 0)) {
                gfx_clear_buffer(&tela);
                if (distancia < 400) {
                    {
                        char buf[32];
                        sprintf(buf, "Distancia: %.3f cm", distancia);
                        gfx_draw_string(&tela, 0, 0, 1, buf);
                    }
                    int barra = (128 * distancia / 300);
                    gfx_draw_line(&tela, 15, 27, barra, 27);
                    gfx_show(&tela);
                } else {
                    gfx_clear_buffer(&tela);
                    gfx_draw_string(&tela, 0, 0, 1, "Sensor Falhou");
                    gfx_show(&tela);
                }
            }
        } else {
            gfx_clear_buffer(&tela);
            gfx_draw_string(&tela, 0, 0, 1, "Sensor Falhou");
            gfx_show(&tela);
        }
    }
}


int main() {
    stdio_init_all();
    xQueueDistance = xQueueCreate(10, sizeof(float));
    xSemaphoreTrigger = xSemaphoreCreateBinary();
    
    xTaskCreate(trigger_task, "Trigger", 256, NULL, 1, NULL);
    xTaskCreate(echo_task, "Echo", 256, NULL, 1, NULL);
    xTaskCreate(oled_task, "OLED", 4096, NULL, 1, NULL);
    
    vTaskStartScheduler();
    while (true)
        ;
}
