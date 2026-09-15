cmake_minimum_required(VERSION 3.20.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Overlay lives in "Board/" (capital B, singular), not Zephyr's
# auto-detected "boards/" convention, so DTC_OVERLAY_FILE must be set
# explicitly before find_package(Zephyr ...).
set(DTC_OVERLAY_FILE ${CMAKE_CURRENT_SOURCE_DIR}/Board/nucleo_g474re.overlay)

# The custom,bldc-motor binding lives in this application, not in Zephyr.
list(APPEND DTS_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(Bldc_Open_Loop_Commutation LANGUAGES CXX)

target_sources(app PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/bldc_driver.cpp
)

target_include_directories(app PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_options(app PRIVATE
  -Wall
  -Wextra
  -Werror
  -Wvolatile
)
