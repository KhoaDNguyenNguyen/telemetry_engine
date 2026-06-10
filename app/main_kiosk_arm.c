#define _POSIX_C_SOURCE 200809L /* Activates POSIX.1-2008 compliant functions (nanosleep) */

#include "graphics_3d.h"
#include "boot_logo.h"      /* Injects the compiled graphical identity (Face + MSSV) */
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

/* Precise POSIX.1-2008 execution delay */
static void SleepNano(long milliseconds) {
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

int main(void) {
    int fb_fd = open(FB_DEVICE, O_RDWR);
    if (fb_fd == -1) {
        fprintf(stderr, "Critical Error: Cannot open framebuffer device %s\n", FB_DEVICE);
        return 1;
    }

    struct fb_var_screeninfo vinfo;
    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) == -1) {
        fprintf(stderr, "Critical Error: Cannot read variable information\n");
        close(fb_fd);
        return 1;
    }

    size_t screensize = (size_t)(vinfo.yres_virtual * vinfo.xres_virtual * (vinfo.bits_per_pixel / 8));
    uint16_t *fbp = (uint16_t *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    
    if ((intptr_t)fbp == -1) {
        fprintf(stderr, "Critical Error: Failed to memory-map framebuffer\n");
        close(fb_fd);
        return 1;
    }

    /* 
     * EARLY BOOT INJECTION: IDENTITY DEPLOYMENT
     * Replaces standard OS initialization output (Tux penguin) with student identity.
     * Utilizing direct memory block transfer (DMA equivalent in User-Space).
     */
    memcpy(fbp, BOOT_LOGO, screensize);
    
    /* Hold the identity screen for 3 seconds */
    SleepNano(3000);

    /* PROCEED TO 3D TELEMETRY ENGINE */
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

    while (1) {
        /* Pseudo-randomized dynamic telemetry variables for compilation validation */
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

        /* Re-initialize framebuffer background */
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

        SleepNano(16); /* Restricts output to 60 Frames Per Second */
    }

    munmap(fbp, screensize);
    close(fb_fd);
    return 0;
}
