cmake_minimum_required(VERSION 3.22)

idf_component_register(
    SRCS
        "main.c" "gfx.c" "fpga_tx.c" "fpga_rx.c" "fpga_common.c" "pwrmgr.c" "cmd_sd_spi.c" "cmd_sd_test.c" "cmd_filesystem.c" "cmd_modules.c" "osd_default.c"
    INCLUDE_DIRS "."
    REQUIRES
        images esp_driver_uart esp_driver_spi esp_driver_gpio nvs_flash fatfs
        common battery button osd menu_mgr tab settings mutex
        console crc sd_spi app_update module_loader embedded_modules
)
