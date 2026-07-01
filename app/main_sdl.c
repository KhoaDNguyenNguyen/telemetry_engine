#include <SDL2/SDL.h>
#include "st7789_logic.h"
#include "graphics_3d.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#define TFT_WIDTH  240
#define TFT_HEIGHT 240
#define PIXEL_COUNT (TFT_WIDTH * TFT_HEIGHT)
#define VERTEX_COUNT 12
#define EDGE_COUNT 12

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static uint16_t frame_buffer[PIXEL_COUNT];

/* --- HARDWARE ABSTRACTION STUBS --- */

static void sdl_write_buffer(const uint16_t *buffer, uint32_t pixel_count) {
    /* Explicitly suppress unused parameter warning */
    (void)pixel_count; 
    
    /* 
     * SDL_PIXELFORMAT_RGB565 interprets memory in native host endianness (Little Endian).
     * No byte swapping is required. The buffer maps 1:1 to the virtual texture.
     */
    SDL_UpdateTexture(texture, NULL, buffer, TFT_WIDTH * sizeof(uint16_t));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

static void stub_cmd(uint8_t cmd) { (void)cmd; }
static void stub_data(uint8_t data) { (void)data; }
static void stub_reset(void) {}
static void stub_delay(uint32_t ms) { SDL_Delay(ms); }

static const spi_driver_ops_t sdl_spi_ops = {
    .write_command = stub_cmd,
    .write_data    = stub_data,
    .write_buffer  = sdl_write_buffer,
    .hard_reset    = stub_reset,
    .delay_ms      = stub_delay
};

/* --- 2D RASTERIZATION ENGINE --- */

static inline uint16_t rgb_to_rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r & 0xF8U) << 8) | ((g & 0xFCU) << 3) | (b >> 3));
}

static inline void draw_pixel(int32_t x, int32_t y, uint16_t color) {
    if (x >= 0 && x < TFT_WIDTH && y >= 0 && y < TFT_HEIGHT) {
        frame_buffer[(uint32_t)y * TFT_WIDTH + (uint32_t)x] = color;
    }
}

static void draw_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t color) {
    for (int32_t i = 0; i < h; i++) {
        for (int32_t j = 0; j < w; j++) {
            draw_pixel(x + j, y + i, color);
        }
    }
}

static void draw_instrument_dial(int32_t xc, int32_t yc, int32_t r, uint16_t ring_color, uint16_t tick_color) {
    int32_t x = 0;
    int32_t y = r;
    int32_t d = 3 - 2 * r;

    while (y >= x) {
        draw_pixel(xc + x, yc + y, ring_color); draw_pixel(xc - x, yc + y, ring_color);
        draw_pixel(xc + x, yc - y, ring_color); draw_pixel(xc - x, yc - y, ring_color);
        draw_pixel(xc + y, yc + x, ring_color); draw_pixel(xc - y, yc + x, ring_color);
        draw_pixel(xc + y, yc - x, ring_color); draw_pixel(xc - y, yc - x, ring_color);
        x++; 
        if (d > 0) { 
            y--; 
            d = d + 4 * (x - y) + 10; 
        } else { 
            d = d + 4 * x + 6; 
        }
    }

    draw_rect(xc - r - 4, yc, 8, 1, tick_color); 
    draw_rect(xc + r - 4, yc, 8, 1, tick_color); 
    draw_rect(xc, yc - r - 4, 1, 8, tick_color); 
    draw_rect(xc, yc + r - 4, 1, 8, tick_color); 
}

static void draw_text_shadowed(int32_t x, int32_t y, const char *text, uint16_t color, uint16_t shadow, uint8_t scale) {
    Graphics3D_DrawString(frame_buffer, TFT_WIDTH, TFT_HEIGHT, (int16_t)(x + 1), (int16_t)(y + 1), text, shadow, scale);
    Graphics3D_DrawString(frame_buffer, TFT_WIDTH, TFT_HEIGHT, (int16_t)x, (int16_t)y, text, color, scale);
}

/* --- MAIN EXECUTION --- */

int main(void) {
    SDL_Init(SDL_INIT_VIDEO);
    window = SDL_CreateWindow("Telemetry Target Simulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, TFT_WIDTH, TFT_HEIGHT, 0);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, TFT_WIDTH, TFT_HEIGHT);

    Vector3D base_verts[VERTEX_COUNT] = {
        {-1.0f,-1.0f,-1.0f,1.0f}, {1.0f,-1.0f,-1.0f,1.0f}, {1.0f,1.0f,-1.0f,1.0f}, {-1.0f,1.0f,-1.0f,1.0f},
        {-1.0f,-1.0f,1.0f,1.0f},  {1.0f,-1.0f,1.0f,1.0f},  {1.0f,1.0f,1.0f,1.0f},  {-1.0f,1.0f,1.0f,1.0f},
        {0.0f,0.0f,0.0f,1.0f},    {1.3f,0.0f,0.0f,1.0f},   {0.0f,1.3f,0.0f,1.0f},  {0.0f,0.0f,1.3f,1.0f}
    };
    
    Vector3D scaled[VERTEX_COUNT], trans[VERTEX_COUNT];
    Point2D proj[VERTEX_COUNT];
    const Edge edges[EDGE_COUNT] = {{0,1},{1,2},{2,3},{3,0}, {4,5},{5,6},{6,7},{7,4}, {0,4},{1,5},{2,6},{3,7}};

    const uint16_t c_white = rgb_to_rgb565(255, 255, 255);
    const uint16_t c_black = rgb_to_rgb565(0, 0, 0);
    const uint16_t c_cyan  = rgb_to_rgb565(0, 255, 255);
    const uint16_t c_mag   = rgb_to_rgb565(255, 0, 255);
    const uint16_t bg      = rgb_to_rgb565(5, 8, 15);
    const uint16_t grid    = rgb_to_rgb565(15, 25, 40);

    float rot_x = 0.0f, rot_y = 0.0f, rot_z = 0.0f, matrix[9];
    float sim_cpu = 0.3f, sim_temp = 45.0f, sim_ram = 0.6f;
    const float dt = 0.033f;

    SDL_Event event;
    int running = 1;
    char buf[64];

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
        }

        for (int32_t i = 0; i < PIXEL_COUNT; i++) frame_buffer[i] = bg;
        for (int32_t x = 10; x < TFT_WIDTH; x += 15) {
            for (int32_t y = 10; y < TFT_HEIGHT; y += 15) {
                draw_pixel(x, y, grid);
            }
        }

        sim_cpu += 0.005f; if (sim_cpu > 1.0f) sim_cpu = 0.1f;
        sim_temp = 40.0f + (sim_cpu * 40.0f);

        float rot_spd = 0.4f + (sim_cpu * 4.0f);
        rot_x += rot_spd * dt; 
        rot_y += rot_spd * 1.3f * dt; 
        rot_z += rot_spd * 0.4f * dt;

        float scale = 0.8f + ((sim_temp - 30.0f) / 100.0f);
        for (int32_t i = 0; i < VERTEX_COUNT; ++i) {
            scaled[i].x = base_verts[i].x * scale; 
            scaled[i].y = base_verts[i].y * scale;
            scaled[i].z = base_verts[i].z * scale; 
            scaled[i].w = 1.0f;
        }

        float norm = (sim_temp - 40.0f) / 45.0f;
        if (norm < 0.0f) norm = 0.0f; 
        if (norm > 1.0f) norm = 1.0f;
        
        uint8_t r = (uint8_t)(norm * 255.0f);
        uint8_t g = (uint8_t)((1.0f - norm) * 255.0f);
        uint8_t b = (uint8_t)((1.0f - norm) * 255.0f + (norm * 50.0f));

        Graphics3D_ComputeRotationMatrix(matrix, rot_x, rot_y, rot_z);
        Graphics3D_RotateVertices(scaled, trans, matrix, VERTEX_COUNT);
        Graphics3D_ProjectVertices(trans, proj, VERTEX_COUNT, TFT_WIDTH, TFT_HEIGHT, 120.0f, 3.0f);

        draw_instrument_dial(TFT_WIDTH/2, TFT_HEIGHT/2, 60, rgb_to_rgb565(r/3, g/3, b/3), c_cyan);

        for (int32_t i = 0; i < EDGE_COUNT; ++i) {
            float intensity = 1.0f - (((trans[edges[i].v1].z + trans[edges[i].v2].z) * 0.5f + 1.5f) / 3.0f);
            if (intensity < 0.15f) intensity = 0.15f; 
            if (intensity > 1.0f) intensity = 1.0f;
            
            uint16_t edge_color = rgb_to_rgb565((uint8_t)(r*intensity), (uint8_t)(g*intensity), (uint8_t)(b*intensity));
            Graphics3D_DrawLine(frame_buffer, TFT_WIDTH, TFT_HEIGHT, 
                                proj[edges[i].v1].x, proj[edges[i].v1].y, 
                                proj[edges[i].v2].x, proj[edges[i].v2].y, 
                                edge_color);
        }

        Point2D c = proj[8];
        Graphics3D_DrawLine(frame_buffer, TFT_WIDTH, TFT_HEIGHT, c.x, c.y, proj[9].x, proj[9].y, rgb_to_rgb565(255, 30, 80));
        Graphics3D_DrawLine(frame_buffer, TFT_WIDTH, TFT_HEIGHT, c.x, c.y, proj[10].x, proj[10].y, rgb_to_rgb565(30, 255, 80));
        Graphics3D_DrawLine(frame_buffer, TFT_WIDTH, TFT_HEIGHT, c.x, c.y, proj[11].x, proj[11].y, rgb_to_rgb565(50, 100, 255));
        draw_rect(c.x - 1, c.y - 1, 3, 3, c_white);

        /* HUD layout padding decoupled from screen edges to avoid corner brackets */
        int32_t l = 15, t = 2;
        draw_rect(5,5,l,t,c_mag); draw_rect(5,5,t,l,c_mag);
        draw_rect(TFT_WIDTH-5-l,5,l,t,c_mag); draw_rect(TFT_WIDTH-5-t,5,t,l,c_mag);
        draw_rect(5,TFT_HEIGHT-5-t,l,t,c_mag); draw_rect(5,TFT_HEIGHT-5-l,t,l,c_mag);
        draw_rect(TFT_WIDTH-5-l,TFT_HEIGHT-5-t,l,t,c_mag); draw_rect(TFT_WIDTH-5-t,TFT_HEIGHT-5-l,t,l,c_mag);

        uint16_t bar_color = rgb_to_rgb565(r, g, b);
        const int32_t pad_x = 24;
        
        snprintf(buf, sizeof(buf), "CPU:%3.0f%%", sim_cpu * 100.0f);
        draw_text_shadowed(pad_x, 10, buf, c_white, c_black, 1);
        draw_rect(pad_x, 20, (int32_t)(sim_cpu * 60.0f), 3, bar_color);

        snprintf(buf, sizeof(buf), "RAM:%3.0f%%", sim_ram * 100.0f);
        draw_text_shadowed(TFT_WIDTH - 85, 10, buf, c_white, c_black, 1);
        draw_rect(TFT_WIDTH - 85, 20, (int32_t)(sim_ram * 60.0f), 3, c_cyan);

        snprintf(buf, sizeof(buf), "TMP:%3.1fC", sim_temp);
        draw_text_shadowed(pad_x, TFT_HEIGHT - 30, buf, bar_color, c_black, 1);

        snprintf(buf, sizeof(buf), "UPT:01:23:45");
        draw_text_shadowed(pad_x, TFT_HEIGHT - 16, buf, c_white, c_black, 1);

        snprintf(buf, sizeof(buf), "TSK:120");
        draw_text_shadowed(TFT_WIDTH - 75, TFT_HEIGHT - 16, buf, c_white, c_black, 1);

        st7789_flush_buffer(&sdl_spi_ops, frame_buffer, TFT_WIDTH, TFT_HEIGHT);
        SDL_Delay(33);
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}