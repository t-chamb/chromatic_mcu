#pragma once

// MCU masters QSPI_ to FPGA
#define PIN_NUM_QSPI_CS          GPIO_NUM_5 // D10
#define PIN_NUM_QSPI_CLK         GPIO_NUM_18 // D9
#define PIN_NUM_QSPI_MOSI        GPIO_NUM_23 // D8
#define PIN_NUM_QSPI_MISO        GPIO_NUM_19 // D7
#define PIN_NUM_QSPI_WP          GPIO_NUM_22 // D6
#define PIN_NUM_QSPI_HD          GPIO_NUM_21 // D5

// I2S
#define PIN_NUM_I2S_BCLK    GPIO_NUM_33 //D16
#define PIN_NUM_I2S_WS      GPIO_NUM_25 //D15
#define PIN_NUM_I2S_DIN     GPIO_NUM_26 //D14
#define PIN_NUM_I2S_DOUT    GPIO_NUM_27 //D13

// Low latency low throughput async uart
#define PIN_NUM_UART_FROM_FPGA  GPIO_NUM_10 //D11
#define PIN_NUM_UART_TO_FPGA    GPIO_NUM_9 //D12

// SPI SD Card Configuration
// NOTE: These pins are remapped from standard ESP32 SD pins to avoid conflicts
// Standard ESP32 SD pins (GPIO6,7,8,11) conflict with flash memory on most boards
// These alternative pins provide reliable SD card operation via SPI mode

// Primary SPI SD Card pins (tested and verified working)
#define PIN_NUM_SD_CS           GPIO_NUM_4  // CS - Chip Select (remapped from GPIO13)
#define PIN_NUM_SD_MOSI         GPIO_NUM_15 // MOSI - Master Out Slave In  
#define PIN_NUM_SD_MISO         GPIO_NUM_2  // MISO - Master In Slave Out
#define PIN_NUM_SD_CLK          GPIO_NUM_14 // CLK - Serial Clock
#define PIN_NUM_SD_POWER        GPIO_NUM_32 // Power control (optional, for clean power cycling)

// Hardware requirements:
// - 10kΩ pull-up resistor on CS (GPIO4) to 3.3V
// - 10kΩ pull-up resistors on SD card DAT1 and DAT2 pins to 3.3V  
// - Power control circuit on GPIO32 (optional but recommended)

// Alternative pins (ESP32 built-in SD pins - may conflict with flash on some boards)
#define PIN_NUM_SD_CS_ALT       GPIO_NUM_11 // SPICS0 (from efuse config)
#define PIN_NUM_SD_MOSI_ALT     GPIO_NUM_8  // SPID (from efuse config)
#define PIN_NUM_SD_MISO_ALT     GPIO_NUM_7  // SPIQ (from efuse config)  
#define PIN_NUM_SD_CLK_ALT      GPIO_NUM_6  // SPICLK (from efuse config)

// Get LVGL SPI handle for shared QSPI access
#include "driver/spi_master.h"
spi_device_handle_t get_lvgl_spi_handle(void);

// Pause/resume LVGL updates (for diagnostic commands that need clean SPI bus)
void lvgl_pause_updates(void);
void lvgl_resume_updates(void);
