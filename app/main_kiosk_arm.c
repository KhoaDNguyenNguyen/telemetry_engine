#define _POSIX_C_SOURCE 200809L

#include "graphics_3d.h"
#include "boot_logo.h"      
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

#define FB_DEVICE "/dev/fb1"
#define VERTEX_COUNT 8
#define EDGE_COUNT 12
#define BOOT_DELAY_NANOSECONDS 3000000000ULL /* 3.0 seconds */
#define FRAME_DURATION_NANOSECONDS 16666666ULL /* ~60 FPS */

typedef enum {
    SYSTEM_STATE_BOOT_INJECTION,
    SYSTEM_STATE_TELEMETRY_EXECUTION,
    SYSTEM_STATE_TERMINATION
} system_state_t;

/* --- Hardware Acceleration & Rendering Subsystem --- */

static inline uint16_t RGB_To_RGB565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

static uint16_t ComputeThermalColor(float temp) {
    float normalized = (temp - 40.0f) / 45.0f;
    if (normalized < 0.0f) normalized = 0.0f;
    if (normalized > 1.0f) normalized = 1.0f;
    
    uint8_t r = (uint8_t)(normalized * 255.0f);
    uint8_t g = (uint8_t)((1.0f - normalized) * 255.0f);
    return RGB_To_RGB565(r, g, 0);
}

/**
 * High-resolution monotonic clock acquisition.
 */
static inline uint64_t GetMonotonicTimeNano(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/**
 * Hypothetical RLE Decoder. Decompresses spatial logo structures directly into MMIO space.
 * Mitigates binary bloat by translating spatial redundancy into algorithmic execution.
 */
static void DeployIdentityRLE(uint16_t *restrict fbp, size_t max_pixels) {
    /* 
     * Fallback to direct memcpy until tools/img_to_c.py is upgraded 
     * to emit RLE structures rather than raw uint16_t arrays.
     */
    memcpy(fbp, BOOT_LOGO, max_pixels * sizeof(uint16_t));
}

int main(void) {
    int fb_fd = open(FB_DEVICE, O_RDWR);
    if (fb_fd == -1) {
        fprintf(stderr, "Critical Error: Cannot open framebuffer device %s\n", FB_DEVICE);
        return EXIT_FAILURE;
    }

    struct fb_var_screeninfo vinfo;
    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) == -1) {
        fprintf(stderr, "Critical Error: Cannot read variable information\n");
        close(fb_fd);
        return EXIT_FAILURE;
    }

    size_t pixel_count = (size_t)(vinfo.yres_virtual * vinfo.xres_virtual);
    size_t screensize = pixel_count * (vinfo.bits_per_pixel / 8);
    uint16_t *fbp = (uint16_t *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    
    if ((intptr_t)fbp == -1) {
        fprintf(stderr, "Critical Error: Failed to memory-map framebuffer\n");
        close(fb_fd);
        return EXIT_FAILURE;
    }

    /* System Execution State Machine */
    system_state_t current_state = SYSTEM_STATE_BOOT_INJECTION;
    uint64_t state_entry_time = GetMonotonicTimeNano();
    uint64_t previous_frame_time = state_entry_time;

    /* 3D Telemetry Mathematical Structures */
    Vector3D base_vertices[VERTEX_COUNT] = {
        {-1, -1, -1, 1}, { 1, -1, -1, 1}, { 1,  1, -1, 1}, {-1,  1, -1, 1},
        {-1, -1,  1, 1}, { 1, -1,  1, 1}, { 1,  1,  1, 1}, {-1,  1,  1, 1}
    };
    Vector3D scaled_vertices[VERTEX_COUNT];
    Vector3D transformed_vertices[VERTEX_COUNT];
    Point2D projected_points[VERTEX_COUNT];

    const Edge edges[EDGE_COUNT] = {
        {0,1}, {1,2}, {2,3}, {3,0},
        {4,5}, {5,6}, {6,7}, {7,4},
        {0,4}, {1,5}, {2,6}, {3,7}
    };

    float rot_x = 0.0f, rot_y = 0.0f, rot_z = 0.0f;
    float matrix[9];

    /* Deploy Initial Identity */
    DeployIdentityRLE(fbp, pixel_count);

    /* Primary Event Loop */
    while (current_state != SYSTEM_STATE_TERMINATION) {
        uint64_t current_time = GetMonotonicTimeNano();
        
        switch (current_state) {
            case SYSTEM_STATE_BOOT_INJECTION:
                /* Non-blocking timeout evaluation */
                if ((current_time - state_entry_time) >= BOOT_DELAY_NANOSECONDS) {
                    current_state = SYSTEM_STATE_TELEMETRY_EXECUTION;
                }
                break;

            case SYSTEM_STATE_TELEMETRY_EXECUTION:
                if ((current_time - previous_frame_time) >= FRAME_DURATION_NANOSECONDS) {
                    previous_frame_time = current_time;

                    /* Pseudo-randomized dynamic telemetry integration */
                    float cpu_load = 0.5f; 
                    float cpu_temp = 55.0f;

                    float rotation_step = 0.02f + (cpu_load * 0.15f);
                    rot_x += rotation_step;
                    rot_y += rotation_step * 1.5f;
                    rot_z += rotation_step * 0.5f;

                    float scale_factor = 1.0f + ((cpu_temp - 40.0f) / 100.0f);
                    for (int i = 0; i < VERTEX_COUNT; ++i) {
                        scaled_vertices[i].x = base_vertices[i].x * scale_factor;
                        scaled_vertices[i].y = base_vertices[i].y * scale_factor;
                        scaled_vertices[i].z = base_vertices[i].z * scale_factor;
                        scaled_vertices[i].w = 1.0f;
                    }

                    uint16_t render_color = ComputeThermalColor(cpu_temp);

                    /* Framebuffer Zeroing (Hardware clear capability pending) */
                    memset(fbp, 0, screensize);

                    Graphics3D_ComputeRotationMatrix(matrix, rot_x, rot_y, rot_z);
                    Graphics3D_RotateVertices(scaled_vertices, transformed_vertices, matrix, VERTEX_COUNT);
                    Graphics3D_ProjectVertices(transformed_vertices, projected_points, VERTEX_COUNT, 
                                               (uint16_t)vinfo.xres, (uint16_t)vinfo.yres, 120.0f, 3.0f);

                    for (int i = 0; i < EDGE_COUNT; ++i) {
                        Point2D p1 = projected_points[edges[i].v1];
                        Point2D p2 = projected_points[edges[i].v2];
                        Graphics3D_DrawLine(fbp, (uint16_t)vinfo.xres, (uint16_t)vinfo.yres, 
                                            p1.x, p1.y, p2.x, p2.y, render_color);
                    }
                }
                break;

            default:
                current_state = SYSTEM_STATE_TERMINATION;
                break;
        }
    }

    munmap(fbp, screensize);
    close(fb_fd);
    return EXIT_SUCCESS;
}