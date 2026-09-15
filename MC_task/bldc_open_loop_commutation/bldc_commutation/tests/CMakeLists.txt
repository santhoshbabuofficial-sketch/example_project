cmake_minimum_required(VERSION 3.20)
project(bldc_host_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.15.2
)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(test_commutation_profile
  test_commutation_profile.cpp
)

target_include_directories(test_commutation_profile PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_compile_options(test_commutation_profile PRIVATE
  -Wall -Wextra -Werror
)

target_link_libraries(test_commutation_profile PRIVATE GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(test_commutation_profile)
