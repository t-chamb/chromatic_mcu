cmake_minimum_required(VERSION 3.22)

idf_component_register(
    SRCS "fpga_psram_module.c"
    INCLUDE_DIRS "."
    REQUIRES module_loader
)
