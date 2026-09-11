cmake_minimum_required(VERSION 3.20.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(Spi_task, CXX)

target_sources(app PRIVATE
    src/main.cpp
)

target_compile_options(app PRIVATE
    -Wall
    -Wextra
    -Werror
   
)
