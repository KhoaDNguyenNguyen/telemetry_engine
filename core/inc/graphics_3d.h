#ifndef GRAPHICS_3D_H
#define GRAPHICS_3D_H

#include <stdint.h>
#include <stddef.h>

typedef struct __attribute__((aligned(16))) {
    float x;
    float y;
    float z;
    float w;
} Vector3D;

typedef struct {
    int16_t x;
    int16_t y;
} Point2D;

typedef struct {
    uint16_t v1;
    uint16_t v2;
} Edge;

void Graphics3D_ComputeRotationMatrix(float *matrix, float angle_x, float angle_y, float angle_z);

void Graphics3D_RotateVertices(const Vector3D *restrict input, Vector3D *restrict output, 
                               const float *restrict matrix, size_t count);

void Graphics3D_ProjectVertices(const Vector3D *restrict input, Point2D *restrict output, 
                                size_t count, uint16_t screen_w, uint16_t screen_h, 
                                float fov, float viewer_distance);

void Graphics3D_DrawLine(uint16_t *restrict fb, uint16_t fb_width, uint16_t fb_height, 
                         int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);

/**
 * @brief Rasterizes an 8x8 bitmap character onto the framebuffer.
 * 
 * @param fb Framebuffer pointer.
 * @param fb_width Framebuffer stride.
 * @param fb_height Framebuffer vertical boundary.
 * @param x Horizontal pixel coordinate.
 * @param y Vertical pixel coordinate.
 * @param c ASCII character to render.
 * @param color RGB565 text color.
 * @param scale Integer scaling factor (1 = 8x8, 2 = 16x16).
 */
void Graphics3D_DrawChar(uint16_t *restrict fb, uint16_t fb_width, uint16_t fb_height, int16_t x, int16_t y, char c, uint16_t color, uint8_t scale);

/**
 * @brief Renders a null-terminated string using the bitmap font.
 */
void Graphics3D_DrawString(uint16_t *restrict fb, uint16_t fb_width, uint16_t fb_height, int16_t x, int16_t y, const char *str, uint16_t color, uint8_t scale);

#endif /* GRAPHICS_3D_H */
