cmake_minimum_required(VERSION 3.20.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Overlay lives in "Board/" (capital B, singular) rather than Zephyr's
# auto-detected "boards/" convention, so DTC_OVERLAY_FILE must be set
# explicitly before find_package(Zephyr ...).
set(DTC_OVERLAY_FILE ${CMAKE_CURRENT_SOURCE_DIR}/Board/nucleo_g474re.overlay)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(Current_PI_Control_Task LANGUAGES CXX)

target_sources(app PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/gpio_overlay.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/current_sensor.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/pi_controller.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/current_control.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/fault_detection.cpp
)

target_compile_options(app PRIVATE
  -Wall
  -Wextra
  -Werror
  -Wvolatile
)

target_include_directories(app PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/Include
)
