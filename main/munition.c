#include "munition.h"
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"

const uint SEL_A_4051 = 13;
const uint SEL_B_4051 = 12;
const uint SEL_C_4051 = 11;
const uint INH_4051   = 10;
const int PIN_LEDS_ON = 27;

void polling_adc_init(void) {
    gpio_init(SEL_A_4051);
    gpio_set_dir(SEL_A_4051, GPIO_OUT);

    gpio_init(SEL_B_4051);
    gpio_set_dir(SEL_B_4051, GPIO_OUT);

    gpio_init(SEL_C_4051); 
    gpio_set_dir(SEL_C_4051, GPIO_OUT);

    gpio_init(INH_4051);         
    gpio_set_dir(INH_4051, GPIO_OUT);
    gpio_put(INH_4051, 1);       // começa desabilitado
}

void select_4051_channel(uint channel) {
    gpio_put(SEL_A_4051, channel & 0x01);
    gpio_put(SEL_B_4051, (channel >> 1) & 0x01);
    gpio_put(SEL_C_4051, (channel >> 2) & 0x01);
}


void munition_show(int show_number){
    stdio_init_all();

    polling_adc_init();

    gpio_init(PIN_LEDS_ON);    // GPIO27 = ADC1
    gpio_set_dir(PIN_LEDS_ON, GPIO_OUT);

    for (uint channel = 0; channel <= show_number; channel++) {
        gpio_put(PIN_LEDS_ON,1);
        // gpio_put(INH_4051, 1);               // desabilita 4051
        select_4051_channel(channel);       // muda canal
        sleep_ms(2);                        // tempo de setup
        gpio_put(INH_4051, 0);               // habilita 4051
        sleep_ms(2);                        // tempo de estabilização
        printf("Canal %d: %d\n", channel, gpio_get(PIN_LEDS_ON));
    }
    printf("--------------------\n");    

}