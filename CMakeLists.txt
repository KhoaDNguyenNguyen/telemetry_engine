cmake_minimum_required(VERSION 3.15)
project(TelemetryEngine VERSION 1.0.0 LANGUAGES C)

# ------------------------------------------------------------------------------
# Build Standard Specifications
# ------------------------------------------------------------------------------
# Enforce the ISO C11 standard strictly. GNU extensions are explicitly disabled 
# to ensure absolute portability to the final Linux Kernel Module (LKM) environment.
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# ------------------------------------------------------------------------------
# Compiler Diagnostics and Static Analysis Flags
# ------------------------------------------------------------------------------
# Implements a zero-tolerance policy for compiler warnings.
# -Wshadow: Prevents variable shadowing which induces logical flaws.
# -Wconversion: Flags implicit type conversions that may truncate data.
# -Wstrict-prototypes / -Wmissing-prototypes: Enforces rigorous function signatures.
set(STRICT_WARNING_FLAGS "-Wall -Wextra -Werror -pedantic -Wstrict-prototypes -Wmissing-prototypes -Wshadow -Wconversion")

# ------------------------------------------------------------------------------
# Extreme Optimization Directives (HPC Configuration)
# ------------------------------------------------------------------------------
# -O3: Enables highest level of compiler optimization.
# -march=native: Instructs the compiler to utilize the maximum instruction set 
#                available on the host processor (e.g., AVX2, AVX-512, NEON).
# -ffast-math: Relaxes IEEE 754 compliance to allow algebraic reassociation, 
#              significantly accelerating trigonometric and matrix multiplications.
# -ftree-vectorize: Forces the compiler to analyze loops for SIMD translation.
# -funroll-loops: Expands loops to minimize branching overhead.
set(OPTIMIZATION_FLAGS "-O3 -march=native -ffast-math -ftree-vectorize -funroll-loops")

# Apply the aggregated flags globally to all C compilations within this project.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${STRICT_WARNING_FLAGS} ${OPTIMIZATION_FLAGS}")

# ------------------------------------------------------------------------------
# Dependency Resolution
# ------------------------------------------------------------------------------
# Locate the Simple DirectMedia Layer (SDL2) library for hardware emulation.
find_package(SDL2 REQUIRED)

# Locate the standard POSIX Math library (libm) required for trigonometric operations 
# (sinf, cosf) utilized in the rotation matrix generation.
find_library(MATH_LIBRARY m)
if(NOT MATH_LIBRARY)
    message(FATAL_ERROR "Standard math library (libm) could not be located. Compilation terminated.")
endif()

# ------------------------------------------------------------------------------
# Target Definitions
# ------------------------------------------------------------------------------

# 1. Core Mathematical and Rendering Engine (Static Library)
# Isolates the platform-agnostic mathematical operations from the hardware interface.
add_library(engine_core STATIC
    core/src/graphics_3d.c
)

# Specify public include directories for the core library to allow structural inheritance.
target_include_directories(engine_core PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/core/inc
)

# 2. SDL2 Hardware Emulation Executable
# Compiles the standalone mockup environment for algorithm validation.
add_executable(test_3d_desktop
    app/main_sdl_test.c
)

# Link all requisite dependencies to the validation executable.
target_link_libraries(test_3d_desktop PRIVATE 
    engine_core 
    ${SDL2_LIBRARIES} 
    ${MATH_LIBRARY}
)

# Propagate SDL2 header paths to the executable.
target_include_directories(test_3d_desktop PRIVATE 
    ${SDL2_INCLUDE_DIRS}
)
# ------------------------------------------------------------------------------
# Software-In-the-Loop (SIL) Testing Interface
# ------------------------------------------------------------------------------
# Compile the mathematical engine and the mock hardware layer into a shared 
# library for Foreign Function Interface (FFI) introspection via Python.

add_library(engine_mock_shared SHARED
    core/src/st7789_logic.c
    hal/mock_hal_x86.c
)

target_include_directories(engine_mock_shared PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/core/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/hal
)

# Enforce standard compliant position-independent code for the shared object.
set_target_properties(engine_mock_shared PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ------------------------------------------------------------------------------
# Production Kiosk Executable (Target: ARM64 / Native Framebuffer)
# ------------------------------------------------------------------------------
# This binary bypasses all graphical environments (X11/Wayland/SDL2) and 
# writes directly to the POSIX memory-mapped hardware interface.

add_executable(kiosk_telemetry_app
    app/main_kiosk_arm.c
)

target_link_libraries(kiosk_telemetry_app PRIVATE 
    engine_core 
    ${MATH_LIBRARY}
)
