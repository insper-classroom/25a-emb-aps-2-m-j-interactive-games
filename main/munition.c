#include "munition.h"

const int LED_PIN_15 = 15;


void munition_show(int show_number){
    stdio_init_all();
    gpio_init(LED_PIN_15);
    gpio_set_dir(LED_PIN_15,GPIO_OUT);
    printf("Foi");
    if(show_number >=1){
        gpio_put(LED_PIN_15,1);

    }

}

