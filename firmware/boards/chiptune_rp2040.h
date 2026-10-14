#pragma once
#define PICO_RP2040 1
#define PICO_BOOT_STAGE2_CHOOSE_W25Q080 1
#define PICO_FLASH_SPI_CLKDIV 4
#ifndef PICO_FLASH_SIZE_BYTES
#define PICO_FLASH_SIZE_BYTES (2 * 1024 * 1024)
#endif
#define PICO_DEFAULT_LED_PIN 28
#define PICO_DEFAULT_I2C 1
#define PICO_DEFAULT_I2C_SDA_PIN 18
#define PICO_DEFAULT_I2C_SCL_PIN 19

/* Route tables and backwards-compatible settings parsing need more than the SDK default. */
#ifndef PICO_STACK_SIZE
#define PICO_STACK_SIZE 0x1000
#endif
