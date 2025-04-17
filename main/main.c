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
#include "munition.h"

const int PIN_ADC_2 = 28;
const int PIN_BTN = 2;


QueueHandle_t xQueueFire;

float moving_average(float *list_data) {
    float soma = 0;
    for (int i = 0; i < 5; i++) {
        soma += list_data[i];
    }
    printf("voltage at spectrasymbol: %f [V]\n", soma/5);

    return soma / 5;
}
volatile int fire = 0;

void gpio_callback(uint gpio, uint32_t events)
{
    if (gpio_get(PIN_BTN) == 1){
        fire = 0;
    }
    else{
        fire = 1;
    }

}

void fire_task(void *p) {
    stdio_init_all();

    gpio_init(PIN_BTN);
    gpio_set_dir(PIN_BTN, GPIO_IN);
    gpio_set_irq_enabled_with_callback(PIN_BTN,
        GPIO_IRQ_EDGE_RISE |
        GPIO_IRQ_EDGE_FALL,
        true,
        &gpio_callback);
    gpio_pull_up(PIN_BTN);

    adc_init();
    adc_gpio_init(PIN_ADC_2);
    
    // 12-bit conversion, assume max value == ADC_VREF == 5 V
    // const float conversion_factor = 5.0f / (1 << 12) ;

    // uint16_t result;
    // float voltage_list[]= {0,0,0,0,0};
    // int voltage_counter = 0;
    int shot = 0;
    while (1) {
        // adc_select_input(3); // Select ADC input 2 (GPIO28)
        // result = adc_read();
        // float voltage = result * conversion_factor;
        // int fire_data = 0;
        // voltage_list[voltage_counter] = voltage;
        // float average_voltage = moving_average(voltage_list);

        // voltage_counter ++;
        // if(voltage_counter >= 5){
            
        //     voltage_counter = 0;
        // }
        // if(average_voltage >= 0.80){
        //     fire_data = 1;
        // }
        // else{
        //     fire_data = 0;
        // }
        
        if(fire){
            shot = 1;
            xQueueSend(xQueueFire, &shot, 0);
            fire =0;
        }
        else{
            shot = 0;
            xQueueSend(xQueueFire, &shot, 0);
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void munition_task(void *p){
    int shot_data;
    int munition_counter = 6;
    while (1) {
        if (xQueueReceive(xQueueFire, &shot_data, portMAX_DELAY)) {
            if(shot_data == 1){
                munition_counter --;
                printf("munition: %d \n",munition_counter);
                if(munition_counter == 0){
                    munition_counter = 6;
                }
            }
            
            munition_show(munition_counter);
        }


    }

}

int main() {
    stdio_init_all();
    adc_init();
    xQueueFire = xQueueCreate(32, sizeof(int));
    xTaskCreate(fire_task, "Fire task", 4095, NULL, 1, NULL);
    xTaskCreate(munition_task, "Munition task", 4095, NULL, 1, NULL);

    vTaskStartScheduler();

    while (true) {
    }
}
