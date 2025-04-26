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
const int PIN_ADC_2 = 28;
const float conversion_factor = 5.0f / (1 << 12);

SemaphoreHandle_t xSemaphoreFire;
QueueHandle_t xQueuePos;

typedef struct dat {
    FusionEuler euler;
} data;

float moving_average(const float *list_data) {
    float soma = 0;
    for (int i = 0; i < 5; i++) {
        soma += list_data[i];
    }
    return soma / 5;
}

void gpio_callback(uint gpio, uint32_t events) {
    if (events & GPIO_IRQ_EDGE_RISE) {
        xSemaphoreGiveFromISR(xSemaphoreFire, NULL);
    }
}

void fire_task(void *p) {
    stdio_init_all();

    gpio_init(PIN_BTN);
    gpio_set_dir(PIN_BTN, GPIO_IN);
    gpio_set_irq_enabled_with_callback(
        PIN_BTN,
        GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
        true,
        &gpio_callback
    );
    gpio_pull_up(PIN_BTN);

    gpio_init(PIN_VIB);
    gpio_set_dir(PIN_VIB, GPIO_OUT);

    while (1) {
        if (xSemaphoreTake(xSemaphoreFire, portMAX_DELAY)) {
            send_uart_packet(2, 20);
            gpio_put(PIN_VIB, 1);
            vTaskDelay(pdMS_TO_TICKS(200));
            gpio_put(PIN_VIB, 0);
        }
    }
}

void recharge_task(void *p) {
    adc_gpio_init(PIN_ADC_2);
    adc_select_input(2);

    float voltage_list[5] = {0};
    int voltage_counter = 0;

    while (1) {
        uint16_t result = adc_read();
        float voltage = result * conversion_factor;

        voltage_list[voltage_counter] = voltage;
        float avg = moving_average(voltage_list);

        voltage_counter = (voltage_counter + 1) % 5;

        if (avg >= 1.80f) {
            for (int i = 0; i < 5; i++) voltage_list[i] = 0.0f;
            send_uart_packet(3, 20);
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

#define SAMPLE_PERIOD (0.01f)
#define MPU_ADDRESS 0x68
#define I2C_SCL_GPIO 21
#define I2C_SDA_GPIO 20

static void mpu6050_reset() {
    uint8_t buf[] = {0x6B, 0x00};
    i2c_write_blocking(i2c_default, MPU_ADDRESS, buf, 2, false);
}

static void mpu6050_read_raw(int16_t accel[3], int16_t gyro[3], int16_t *temp) {
    uint8_t buffer[6];
    uint8_t reg;

    reg = 0x3B;
    i2c_write_blocking(i2c_default, MPU_ADDRESS, &reg, 1, true);
    i2c_read_blocking(i2c_default, MPU_ADDRESS, buffer, 6, false);
    for (int i = 0; i < 3; i++) {
        accel[i] = (int16_t)(buffer[i * 2] << 8 | buffer[i * 2 + 1]);
    }

    reg = 0x43;
    i2c_write_blocking(i2c_default, MPU_ADDRESS, &reg, 1, true);
    i2c_read_blocking(i2c_default, MPU_ADDRESS, buffer, 6, false);
    for (int i = 0; i < 3; i++) {
        gyro[i] = (int16_t)(buffer[i * 2] << 8 | buffer[i * 2 + 1]);
    }

    reg = 0x41;
    i2c_write_blocking(i2c_default, MPU_ADDRESS, &reg, 1, true);
    i2c_read_blocking(i2c_default, MPU_ADDRESS, buffer, 2, false);
    *temp = (int16_t)(buffer[0] << 8 | buffer[1]);
}

void mpu6050_task(void *p) {
    i2c_init(i2c_default, 400 * 1000);
    gpio_set_function(I2C_SDA_GPIO, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_GPIO, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_GPIO);
    gpio_pull_up(I2C_SCL_GPIO);

    mpu6050_reset();
    int16_t acceleration[3], gyro[3], temp;
    FusionAhrs ahrs;
    FusionAhrsInitialise(&ahrs);

    float v_yaw_prev = 0.0f;
    float v_pitch_prev = 0.0f;
    float v_roll_prev = 0.0f;
    float yaw_offset = 0.0f;
    bool yaw_offset_set = false;

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
        if (euler.angle.pitch > -10.0f && euler.angle.pitch < -8.5f) euler.angle.pitch = 0;
        if (euler.angle.roll > -3.5f && euler.angle.roll < -1.0f) euler.angle.roll = 0;

        float d_roll  = fabsf(euler.angle.roll  - v_roll_prev);
        float d_pitch = fabsf(euler.angle.pitch - v_pitch_prev);
        float d_yaw   = fabsf(euler.angle.yaw   - v_yaw_prev);

        if (!yaw_offset_set && d_pitch < 1.0f) {
            yaw_offset = euler.angle.yaw;
            yaw_offset_set = true;
        }
        euler.angle.yaw -= yaw_offset;

        if (d_roll >= 1.0f || d_pitch >= 1.0f || d_yaw >= 1.0f) {
            v_roll_prev  = euler.angle.roll;
            v_pitch_prev = euler.angle.pitch;
            v_yaw_prev   = euler.angle.yaw;
            data position = { .euler = euler };
            xQueueSend(xQueuePos, &position, portMAX_DELAY);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

#define UART_ID    uart0
#define BAUD_RATE  115200
#define UART_TX_PIN 0
#define UART_RX_PIN 1

void send_uart_packet(uint8_t axis, int32_t valor) {
    if (valor == 0) return;
    uint32_t uvalor = (uint32_t) valor;
    uint8_t bytes[4];
    bytes[0] = axis;
    bytes[1] = (uvalor >> 8) & 0xFF;
    bytes[2] = uvalor & 0xFF;
    bytes[3] = 0xFF;
    uart_write_blocking(HC06_UART_ID, bytes, 4);
}

void uart_task(void *p) {
    data pin_data;
    uart_init(HC06_UART_ID, HC06_BAUD_RATE);
    gpio_set_function(HC06_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(HC06_RX_PIN, GPIO_FUNC_UART);
    hc06_init("desert_eagle ", "FIRE");

    while (1) {
        if (xQueueReceive(xQueuePos, &pin_data, portMAX_DELAY)) {
            if (pin_data.euler.angle.roll != 0) {
                int axis = 1; // roll
                int32_t val = (int32_t) pin_data.euler.angle.roll;
                send_uart_packet(axis, val);
            }
            if (pin_data.euler.angle.pitch != 0) {
                int axis = 0; // pitch
                int32_t val = - (int32_t) pin_data.euler.angle.pitch;
                send_uart_packet(axis, val);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

int main() {
    stdio_init_all();
    adc_init();

    xQueuePos = xQueueCreate(32, sizeof(data));
    xSemaphoreFire = xSemaphoreCreateBinary();

    xTaskCreate(fire_task, "Fire task", 4096, NULL, 1, NULL);
    xTaskCreate(mpu6050_task, "MPU6050 task", 8192, NULL, 1, NULL);
    xTaskCreate(uart_task, "UART task", 4096, NULL, 1, NULL);
    xTaskCreate(recharge_task, "Recharge task", 4096, NULL, 1, NULL);

    vTaskStartScheduler();
    while (1) {}
    return 0;
}