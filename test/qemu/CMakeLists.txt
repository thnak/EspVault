# QEMU Test Project for EspVault
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Set target to ESP32 for QEMU testing
set(IDF_TARGET "esp32")

project(espvault_qemu_test)
