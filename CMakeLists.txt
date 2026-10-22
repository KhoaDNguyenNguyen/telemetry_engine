cmake_minimum_required(VERSION 3.15)
project(TelemetryEngine VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(STRICT_WARNING_FLAGS "-Wall -Wextra -Werror -pedantic -Wstrict-prototypes -Wmissing-prototypes -Wshadow -Wconversion")
set(OPTIMIZATION_FLAGS "-O3 -ffast-math -ftree-vectorize -funroll-loops")

set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
set(CMAKE_EXE_LINKER_FLAGS "-static")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${STRICT_WARNING_FLAGS} ${OPTIMIZATION_FLAGS}")

add_library(engine_core OBJECT core/src/graphics_3d.c core/src/st7789_logic.c)
set_target_properties(engine_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(engine_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/core/inc ${CMAKE_CURRENT_SOURCE_DIR}/hal)

add_library(engine_mock_shared SHARED hal/mock_hal_x86.c)
target_link_libraries(engine_mock_shared PRIVATE engine_core m)
set_target_properties(engine_mock_shared PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_executable(kiosk_telemetry_app app/main.c hal/hal_rpi.c)
target_link_libraries(kiosk_telemetry_app PRIVATE engine_core -static m)
set_target_properties(kiosk_telemetry_app PROPERTIES LINK_FLAGS "-static")
