cmake_minimum_required(VERSION 3.22)

idf_component_register(SRCS "sd_spi.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_driver_spi fatfs
                    PRIV_REQUIRES main)