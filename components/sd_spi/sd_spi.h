#pragma once

#include "esp_err.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool mounted;
    sdmmc_card_t *card;
    char mount_point[16];
} sd_spi_context_t;

esp_err_t sd_spi_init(void);
esp_err_t sd_spi_init_alt_pins(void);  // Force use alternative pins
esp_err_t sd_spi_mount(const char *mount_point);
esp_err_t sd_spi_unmount(void);
esp_err_t sd_spi_deinit(void);
bool sd_spi_is_mounted(void);
sdmmc_card_t* sd_spi_get_card_info(void);

#ifdef __cplusplus
}
#endif