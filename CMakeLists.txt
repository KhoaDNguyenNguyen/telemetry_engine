cmake_minimum_required(VERSION 3.15)
project(TelemetryEngine VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(STRICT_WARNING_FLAGS "-Wall -Wextra -Werror -pedantic -Wstrict-prototypes -Wmissing-prototypes -Wshadow -Wconversion")
set(OPTIMIZATION_FLAGS "-O3 -ffast-math -ftree-vectorize -funroll-loops")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${STRICT_WARNING_FLAGS} ${OPTIMIZATION_FLAGS}")

find_library(MATH_LIBRARY m REQUIRED)

# 1. Core Logic Library (Hardware Agnostic)
add_library(engine_core OBJECT 
    core/src/graphics_3d.c 
    core/src/st7789_logic.c
)
set_target_properties(engine_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(engine_core PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/core/inc 
    ${CMAKE_CURRENT_SOURCE_DIR}/hal
)

# 2. Software-In-the-Loop Mock Target (x86_64 Host)
add_library(engine_mock_shared SHARED 
    hal/mock_hal_x86.c
)
target_link_libraries(engine_mock_shared PRIVATE engine_core ${MATH_LIBRARY})
set_target_properties(engine_mock_shared PROPERTIES POSITION_INDEPENDENT_CODE ON)

# 3. Target Executable (AArch64 bare-metal/PID 1)
add_executable(kiosk_telemetry_app 
    app/main.c 
    hal/hal_rpi.c
)
target_link_libraries(kiosk_telemetry_app PRIVATE engine_core ${MATH_LIBRARY})

# 4. Host Desktop Simulator (SDL2)
find_package(SDL2 REQUIRED)
add_executable(simulator_desktop app/main_sdl.c)
target_link_libraries(simulator_desktop PRIVATE engine_core ${MATH_LIBRARY} ${SDL2_LIBRARIES})
target_include_directories(simulator_desktop PRIVATE ${SDL2_INCLUDE_DIRS})