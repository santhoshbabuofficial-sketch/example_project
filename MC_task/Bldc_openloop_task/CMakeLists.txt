cmake_minimum_required(VERSION 3.20.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The overlay lives in "Board/" (capital B, singular), which is not
# Zephyr's auto-detected "boards/" convention, so DTC_OVERLAY_FILE must be
# set explicitly - and before find_package(Zephyr ...).
set(DTC_OVERLAY_FILE ${CMAKE_CURRENT_SOURCE_DIR}/Board/nucleo_g474re.overlay)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(Bldc_OpenLoop_Task LANGUAGES CXX)

target_sources(app PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/gpio_overlay.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/bldc_driver.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/speed_ramp.cpp
)

target_compile_options(app PRIVATE
  -Wall
  -Wextra
  -Werror
  -Wvolatile
)

# Folder on disk is "Include/" (capital I) - case-sensitive on Linux.
target_include_directories(app PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/Include
)
