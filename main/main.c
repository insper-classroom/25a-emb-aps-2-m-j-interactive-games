/*
 * LED blink with FreeRTOS
 */
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "mpu6050.h"
#include "Fusion.h"
#include "ssd1306.h"
#include "gfx.h"

#include "pico/stdlib.h"
#include <stdio.h>
#include "hardware/adc.h"
#include <inttypes.h>
#include "hc06.h"

const int PIN_BTN = 2;
const int PIN_VIB = 17;



float moving_average(float *list_data) {
    float soma = 0;
    for (int i = 0; i < 5; i++) {
        soma += list_data[i];
    }
    return soma / 5;
}
SemaphoreHandle_t xSemaphoreFire;

void gpio_callback(uint gpio, uint32_t events)
{

    if (events & GPIO_IRQ_EDGE_RISE) {
        xSemaphoreGiveFromISR(xSemaphoreFire, 0);
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
    gpio_init(PIN_VIB);
    gpio_set_dir(PIN_VIB, GPIO_OUT);
    int shot = 0;

    while (1) {
        
        if (xSemaphoreTake(xSemaphoreFire, portMAX_DELAY)) {
            shot = 1;
            send_uart_packet(2, 20);            
            gpio_put(PIN_VIB,1);
            vTaskDelay(pdMS_TO_TICKS(200));
            gpio_put(PIN_VIB,0);
            
        }

    }
}




const int PIN_ADC_2 = 28;
const float conversion_factor = 5.0f / (1 << 12) ;

uint16_t result;
float voltage_list[]= {0,0,0,0,0};
int voltage_counter = 0;
void recharge_task(void *p){
    adc_gpio_init(PIN_ADC_2);
    adc_select_input(2); // Deve ser '2' para GPIO28 (veja observação abaixo)

    while (1) {
        result = adc_read();
        float voltage = result * conversion_factor;
        int recharge_data = 0;

        voltage_list[voltage_counter] = voltage;
        float average_voltage = moving_average(voltage_list);

        voltage_counter++;
        if(voltage_counter >= 5){
            voltage_counter = 0;
        }

        if(average_voltage >= 1.80){
            voltage_list[0]= 0;
            voltage_list[1]= 0;
            voltage_list[2]= 0;
            voltage_list[3]= 0;
            voltage_list[4]= 0;

            recharge_data = 1;
            send_uart_packet(3, 20);
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
#define SAMPLE_PERIOD (0.01f) // replace this with actual sample period

const int MPU_ADDRESS = 0x68;
const int I2C_SCL_GPIO = 21;
const int I2C_SDA_GPIO = 20;
QueueHandle_t xQueuePos;
static void mpu6050_reset() {
    // Two byte reset. First byte register, second byte data
    // There are a load more options to set up the device in different ways that could be added here
    uint8_t buf[] = {0x6B, 0x00};
    i2c_write_blocking(i2c_default, MPU_ADDRESS, buf, 2, false);
}

static void mpu6050_read_raw(int16_t accel[3], int16_t gyro[3], int16_t *temp) {
    // For this particular device, we send the device the register we want to read
    // first, then subsequently read from the device. The register is auto incrementing
    // so we don't need to keep sending the register we want, just the first.

    uint8_t buffer[6];

    // Start reading acceleration registers from register 0x3B for 6 bytes
    uint8_t val = 0x3B;
    i2c_write_blocking(i2c_default, MPU_ADDRESS, &val, 1, true); // true to keep master control of bus
    i2c_read_blocking(i2c_default, MPU_ADDRESS, buffer, 6, false);

    for (int i = 0; i < 3; i++) {
        accel[i] = (buffer[i * 2] << 8 | buffer[(i * 2) + 1]);
    }

    // Now gyro data from reg 0x43 for 6 bytes
    // The register is auto incrementing on each read
    val = 0x43;
    i2c_write_blocking(i2c_default, MPU_ADDRESS, &val, 1, true);
    i2c_read_blocking(i2c_default, MPU_ADDRESS, buffer, 6, false);  // False - finished with bus

    for (int i = 0; i < 3; i++) {
        gyro[i] = (buffer[i * 2] << 8 | buffer[(i * 2) + 1]);;
    }

    // Now temperature from reg 0x41 for 2 bytes
    // The register is auto incrementing on each read
    val = 0x41;
    i2c_write_blocking(i2c_default, MPU_ADDRESS, &val, 1, true);
    i2c_read_blocking(i2c_default, MPU_ADDRESS, buffer, 2, false);  // False - finished with bus

    *temp = buffer[0] << 8 | buffer[1];
}
typedef struct dat {
    FusionEuler euler;
    int click;
 } data;
 void mpu6050_task(void *p) {
    // Configuração do I2C
    i2c_init(i2c_default, 400 * 1000);
    gpio_set_function(I2C_SDA_GPIO, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_GPIO, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_GPIO);
    gpio_pull_up(I2C_SCL_GPIO);

    mpu6050_reset();
    int16_t acceleration[3], gyro[3], temp;
    FusionAhrs ahrs;
    FusionAhrsInitialise(&ahrs);

    float v_yaw_anterior = 0.0f;
    float v_pitch_anterior = 0.0f;
    float v_roll_anterior = 0.0f;

    // Novo: variáveis de offset
    static float yaw_offset = 0.0f;
    static bool yaw_offset_set = false;

    while (1) {
        mpu6050_read_raw(acceleration, gyro, &temp);

        FusionVector gyroscope = {
            .axis.x = gyro[0] / 131.0f,
            .axis.y = gyro[1] / 131.0f,
            .axis.z = gyro[2] / 131.0f,
        };

        FusionVector accelerometer = {
            .axis.x = acceleration[0] / 16384.0f,
            .axis.y = acceleration[1] / 16384.0f,
            .axis.z = acceleration[2] / 16384.0f,
        };

        FusionAhrsUpdateNoMagnetometer(&ahrs, gyroscope, accelerometer, SAMPLE_PERIOD);
        FusionEuler euler = FusionQuaternionToEuler(FusionAhrsGetQuaternion(&ahrs));

        // Zera pequenos ruídos no pitch e roll
        if(euler.angle.pitch < -8.5 && euler.angle.pitch > -10){
            euler.angle.pitch = 0;
        }
        if(euler.angle.roll < -1 && euler.angle.roll > -3.5){
            euler.angle.roll = 0;
        }

        float delta_roll  = fabsf(euler.angle.roll  - v_roll_anterior);
        float delta_pitch = fabsf(euler.angle.pitch - v_pitch_anterior);
        float delta_yaw   = fabsf(euler.angle.yaw   - v_yaw_anterior);

        if (!yaw_offset_set && delta_pitch < 1.0f) {
            yaw_offset = euler.angle.yaw;
            yaw_offset_set = true;
        }
        euler.angle.yaw -= yaw_offset;
        if (delta_roll >= 1.0f || delta_pitch >= 1.0f || delta_yaw >= 1.0f) {
            v_roll_anterior  = euler.angle.roll;
            v_pitch_anterior = euler.angle.pitch;
            v_yaw_anterior   = euler.angle.yaw;

            data position;
            position.euler = euler;

            xQueueSend(xQueuePos, &position, 0);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}


#define UART_ID uart0
#define BAUD_RATE 115200
#define UART_TX_PIN 0
#define UART_RX_PIN 1



void send_uart_packet(uint8_t axis, int32_t valor) {
    if (valor == 0) return;
    if(axis == 0 && valor < -1 && valor >-7) return;
    uint8_t bytes[4];
    bytes[0] = axis;
    bytes[1] = (valor >> 8) & 0xFF;
    bytes[2] = valor & 0xFF;
    bytes[3] = 0xFF;
    uart_write_blocking(HC06_UART_ID, bytes, 4);
}

void uart_task(void *p) {

    data pin_data;
    int32_t last_pitch_sent = 0;
    int32_t last_yaw_sent = 0;
    int32_t variation = 0;
    uart_init(HC06_UART_ID, HC06_BAUD_RATE);
    gpio_set_function(HC06_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(HC06_RX_PIN, GPIO_FUNC_UART);
    hc06_init("desert_eagle ", "FIRE");
    int shot_data;
    int recharge_data;
    while (1) {

        if (xQueueReceive(xQueuePos, &pin_data, portMAX_DELAY)) {

            int axis_z = 1;  // Roll
            int axis_y = 0;  // pitch
            int fire =2;

            // Roll
            if (pin_data.euler.angle.roll != 0) {
                int32_t val_z = (int32_t)(pin_data.euler.angle.roll);
                if (abs(val_z - last_pitch_sent) >= 2) {
                    send_uart_packet(axis_z, val_z);
                    last_pitch_sent = val_z;
                }
            }

            // Pitch
            if (pin_data.euler.angle.pitch != 0) {
                int32_t val_y = -1*(int32_t)(pin_data.euler.angle.pitch);
                if (abs(val_y - last_yaw_sent) >= 5) {
                    send_uart_packet(axis_y, val_y);
                    last_yaw_sent = val_y;
                }
            }
        }


        vTaskDelay(pdMS_TO_TICKS(10)); // pequena pausa

    }
}
int main() {
    stdio_init_all();
    adc_init();
    xQueuePos = xQueueCreate(32, sizeof(data));
    xSemaphoreFire = xSemaphoreCreateBinary();

    xTaskCreate(fire_task, "Fire task", 4095, NULL, 1, NULL);
    xTaskCreate(mpu6050_task, "mpu6050_Task 1", 8192, NULL, 1, NULL);
    xTaskCreate(uart_task, "uart_task", 4095, NULL, 1, NULL);
    xTaskCreate(recharge_task, "recharge TASK", 4095, NULL, 1, NULL);

    vTaskStartScheduler();

    while (true) {
    }
}
