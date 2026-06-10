#include "graphics_3d.h"
#include "boot_logo.h"
#include <SDL2/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#define TFT_WIDTH 240
#define TFT_HEIGHT 240
#define VERTEX_COUNT 8
#define EDGE_COUNT 12

uint16_t framebuffer[TFT_WIDTH * TFT_HEIGHT];

static inline uint16_t RGB_To_RGB565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

int main(void) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "HPC 3D Telemetry Engine Mockup",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        TFT_WIDTH, TFT_HEIGHT, SDL_WINDOW_SHOWN
    );

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_Texture* texture = SDL_CreateTexture(
        renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, 
        TFT_WIDTH, TFT_HEIGHT
    );

    /* --- PHASE 1: EARLY BOOT IDENTITY INJECTION --- */
    
    /* Copy the actual facial identity binary array */
    memcpy(framebuffer, BOOT_LOGO, sizeof(framebuffer));

    /* Typographic Configuration */
    const uint16_t text_color_primary = RGB_To_RGB565(255, 255, 255); /* White */
    const uint16_t text_color_accent  = RGB_To_RGB565(0, 255, 128);   /* Cyberpunk Green */

    /* Overlay Identity Data */
    /* Scale 1 (8x8 pixels per char). Centered horizontally roughly */
    Graphics3D_DrawString(framebuffer, TFT_WIDTH, TFT_HEIGHT, 44, 200, "Dang-Khoa N. Nguyen", text_color_primary, 1);
    Graphics3D_DrawString(framebuffer, TFT_WIDTH, TFT_HEIGHT, 76, 215, "ID: 24161099", text_color_accent, 1);

    SDL_UpdateTexture(texture, NULL, framebuffer, TFT_WIDTH * sizeof(uint16_t));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);

    /* Hold identity matrix for 3000 milliseconds */
    SDL_Delay(3000);

    /* --- PHASE 2: TELEMETRY ENGINE SUBSYSTEM --- */
    
    Vector3D vertices[VERTEX_COUNT] = {
        {-1, -1, -1, 1}, { 1, -1, -1, 1}, { 1,  1, -1, 1}, {-1,  1, -1, 1},
        {-1, -1,  1, 1}, { 1, -1,  1, 1}, { 1,  1,  1, 1}, {-1,  1,  1, 1}
    };
    
    Vector3D transformed_vertices[VERTEX_COUNT];
    Point2D projected_points[VERTEX_COUNT];

    const Edge edges[EDGE_COUNT] = {
        {0,1}, {1,2}, {2,3}, {3,0},
        {4,5}, {5,6}, {6,7}, {7,4},
        {0,4}, {1,5}, {2,6}, {3,7}
    };

    float rot_x = 0.0f, rot_y = 0.0f, rot_z = 0.0f;
    float matrix[9];
    bool running = true;
    SDL_Event event;

    const uint16_t bg_color = RGB_To_RGB565(10, 10, 10);

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }

        #pragma GCC unroll 8
        for (int i = 0; i < TFT_WIDTH * TFT_HEIGHT; i++) {
            framebuffer[i] = bg_color;
        }

        rot_x += 0.02f;
        rot_y += 0.03f;
        rot_z += 0.01f;

        Graphics3D_ComputeRotationMatrix(matrix, rot_x, rot_y, rot_z);
        Graphics3D_RotateVertices(vertices, transformed_vertices, matrix, VERTEX_COUNT);
        Graphics3D_ProjectVertices(transformed_vertices, projected_points, VERTEX_COUNT, 
                                   TFT_WIDTH, TFT_HEIGHT, 120.0f, 3.0f);

        for (int i = 0; i < EDGE_COUNT; ++i) {
            Point2D p1 = projected_points[edges[i].v1];
            Point2D p2 = projected_points[edges[i].v2];
            Graphics3D_DrawLine(framebuffer, TFT_WIDTH, TFT_HEIGHT, p1.x, p1.y, p2.x, p2.y, text_color_accent);
        }

        /* Persistent Telemetry Overlay Overlay */
        Graphics3D_DrawString(framebuffer, TFT_WIDTH, TFT_HEIGHT, 10, 10, "SYS: NOMINAL", text_color_primary, 1);
        Graphics3D_DrawString(framebuffer, TFT_WIDTH, TFT_HEIGHT, 10, 25, "OP: D. KHOA", text_color_accent, 1);

        SDL_UpdateTexture(texture, NULL, framebuffer, TFT_WIDTH * sizeof(uint16_t));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);

        SDL_Delay(16);
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
