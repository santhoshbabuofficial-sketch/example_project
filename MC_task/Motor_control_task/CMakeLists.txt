cmake_minimum_required(VERSION 3.20.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# NOTE: the overlay lives in "Board/" (capital B, singular), which is not
# Zephyr's auto-detected "boards/" convention, and nothing previously set
# DTC_OVERLAY_FILE - meaning the overlay was silently never applied. Must
# be set before find_package(Zephyr ...).
set(DTC_OVERLAY_FILE ${CMAKE_CURRENT_SOURCE_DIR}/Board/nucleo_g474re.overlay)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(Motor_Control_Task LANGUAGES CXX)

target_sources(app PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/gpio_overlay.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/voltage_sensor.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/current_sensor.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/motor_control.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/sensor_manager.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/lcd_display.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/fault_detection.cpp
)

target_compile_options(app PRIVATE
  -Wall
  -Wextra
  -Werror
  -Wvolatile
)

# NOTE: this was previously "include/" (lowercase). The folder on disk is
# "Include/" (capital I), which is case-sensitive on Linux and would fail
# to build as-is - fixed here.
target_include_directories(app PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/Include
)
