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
#include "hardware/adc.h"


const int PIN_ADC_2 = 28;

QueueHandle_t xQueueFire;

float moving_average(float *list_data) {
    float soma = 0;
    for (int i = 0; i < 5; i++) {
        soma += list_data[i];
    }
    printf("voltage at spectrasymbol: %f [V]\n", soma/5);

    return soma / 5;
}

void fire_task(void *p) {
    adc_init();
    adc_gpio_init(PIN_ADC_2);
    // 12-bit conversion, assume max value == ADC_VREF == 5 V
    const float conversion_factor = 5.0f / (1 << 12) ;

    uint16_t result;
    float voltage_list[]= {0,0,0,0,0};
    int voltage_counter = 0;
    while (1) {
        adc_select_input(3); // Select ADC input 2 (GPIO28)
        result = adc_read();
        float voltage = result * conversion_factor;
        int fire_data = 0;
        voltage_list[voltage_counter] = voltage;
        float average_voltage = moving_average(voltage_list);

        voltage_counter ++;
        if(voltage_counter >= 5){
            
            voltage_counter = 0;
        }
        if(average_voltage >= 0.6100){
            fire_data = 1;
        }
        else{
            fire_data = 0;
        }
        

        xQueueSend(xQueueFire, &fire_data, 0);

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

int main() {
    stdio_init_all();
    adc_init();
    xQueueFire = xQueueCreate(32, sizeof(int));
    xTaskCreate(fire_task, "Fire task", 4095, NULL, 1, NULL);
    vTaskStartScheduler();

    while (true) {
    }
}
