cmake_minimum_required(VERSION 3.22)

idf_component_register(
    SRCS "module_loader.c"
    INCLUDE_DIRS "."
    REQUIRES spi_flash fatfs
)
