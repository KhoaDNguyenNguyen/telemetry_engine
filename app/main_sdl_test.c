#include "graphics_3d.h"
#include "boot_logo.h"
#include <SDL2/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#define TFT_WIDTH 240
#define TFT_HEIGHT 240
#define VERTEX_COUNT 12
#define EDGE_COUNT 12

uint16_t framebuffer[TFT_WIDTH * TFT_HEIGHT];

/* --- REAL TELEMETRY SENSORS (LINUX VFS) --- */
static float Read_CPUTemperature(void) {
    char path[64]; int max_temp = 0;
    for (int i = 0; i < 5; i++) {
        snprintf(path, sizeof(path), "/sys/class/thermal/thermal_zone%d/temp", i);
        FILE *fd = fopen(path, "r");
        if (fd) {
            int temp;
            if (fscanf(fd, "%d", &temp) == 1 && temp > max_temp && temp < 150000) {
                max_temp = temp;
            }
            fclose(fd);
        }
    }
    return max_temp > 0 ? (float)max_temp / 1000.0f : 40.0f;
}

static float Read_CPULoad(void) {
    static unsigned long long prev_user, prev_nice, prev_system, prev_idle;
    unsigned long long user, nice, system, idle;
    FILE *fd = fopen("/proc/stat", "r");
    if (!fd) return 0.0f;
    if (fscanf(fd, "cpu  %llu %llu %llu %llu", &user, &nice, &system, &idle) != 4) { 
        fclose(fd); 
        return 0.0f; 
    }
    fclose(fd);
    unsigned long long total_diff = (user + nice + system + idle) - (prev_user + prev_nice + prev_system + prev_idle);
    unsigned long long idle_diff = idle - prev_idle;
    prev_user = user; prev_nice = nice; prev_system = system; prev_idle = idle;
    return total_diff == 0 ? 0.0f : (float)(total_diff - idle_diff) / (float)total_diff;
}

static float Read_RAMUsage(void) {
    FILE *fd = fopen("/proc/meminfo", "r");
    if (!fd) return 0.0f;
    long total = 0, available = 0; char line[256];
    while (fgets(line, sizeof(line), fd)) {
        if (strncmp(line, "MemTotal:", 9) == 0) sscanf(line, "MemTotal: %ld kB", &total);
        if (strncmp(line, "MemAvailable:", 13) == 0) sscanf(line, "MemAvailable: %ld kB", &available);
    }
    fclose(fd);
    return total == 0 ? 0.0f : (float)(total - available) / (float)total;
}

static void Read_Uptime(int *hours, int *mins, int *secs) {
    FILE *fd = fopen("/proc/uptime", "r");
    if (fd) {
        float uptime;
        if (fscanf(fd, "%f", &uptime) == 1) {
            int total_secs = (int)uptime;
            *hours = total_secs / 3600;
            *mins = (total_secs % 3600) / 60;
            *secs = total_secs % 60;
        }
        fclose(fd);
    }
}

static int Read_TotalTasks(void) {
    FILE *fd = fopen("/proc/loadavg", "r");
    int running = 0, total = 0;
    if (fd) {
        if (fscanf(fd, "%*f %*f %*f %d/%d", &running, &total) != 2) {
            total = 0;
        }
        fclose(fd);
    }
    return total;
}

/* --- GRAPHICS SUBSYSTEM --- */
static inline uint16_t RGB_To_RGB565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

static inline void Draw_Pixel(int x, int y, uint16_t color) {
    if (x >= 0 && x < TFT_WIDTH && y >= 0 && y < TFT_HEIGHT) {
        framebuffer[y * TFT_WIDTH + x] = color;
    }
}

static void Draw_Rect(int x, int y, int w, int h, uint16_t color) {
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            Draw_Pixel(x + j, y + i, color);
        }
    }
}

static void Draw_Instrument_Dial(int xc, int yc, int r, uint16_t ring_color, uint16_t tick_color) {
    int x = 0, y = r;
    int d = 3 - 2 * r;
    while (y >= x) {
        Draw_Pixel(xc+x, yc+y, ring_color); Draw_Pixel(xc-x, yc+y, ring_color);
        Draw_Pixel(xc+x, yc-y, ring_color); Draw_Pixel(xc-x, yc-y, ring_color);
        Draw_Pixel(xc+y, yc+x, ring_color); Draw_Pixel(xc-y, yc+x, ring_color);
        Draw_Pixel(xc+y, yc-x, ring_color); Draw_Pixel(xc-y, yc-x, ring_color);
        x++; 
        if (d > 0) { 
            y--; 
            d = d + 4 * (x - y) + 10; 
        } else { 
            d = d + 4 * x + 6; 
        }
    }
    Draw_Rect(xc - r - 4, yc, 8, 1, tick_color); 
    Draw_Rect(xc + r - 4, yc, 8, 1, tick_color); 
    Draw_Rect(xc, yc - r - 4, 1, 8, tick_color); 
    Draw_Rect(xc, yc + r - 4, 1, 8, tick_color); 
}

static void Draw_Text_Shadowed(int x, int y, const char *text, uint16_t text_color, uint16_t shadow_color, uint8_t scale) {
    Graphics3D_DrawString(framebuffer, TFT_WIDTH, TFT_HEIGHT, (int16_t)(x + 1), (int16_t)(y + 1), text, shadow_color, scale);
    Graphics3D_DrawString(framebuffer, TFT_WIDTH, TFT_HEIGHT, (int16_t)x, (int16_t)y, text, text_color, scale);
}

int main(void) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
    SDL_Window* window = SDL_CreateWindow("HPC 3D Telemetry HUD", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, TFT_WIDTH, TFT_HEIGHT, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, TFT_WIDTH, TFT_HEIGHT);

    const uint16_t c_white = RGB_To_RGB565(255, 255, 255), c_black = RGB_To_RGB565(0, 0, 0);
    const uint16_t c_cyan  = RGB_To_RGB565(0, 255, 255), c_mag = RGB_To_RGB565(255, 0, 255);

    /* --- EARLY BOOT INJECTION --- */
    memcpy(framebuffer, BOOT_LOGO, sizeof(framebuffer));
    Draw_Text_Shadowed(44, 200, "Dang-Khoa N. Nguyen", c_white, c_black, 1);
    Draw_Text_Shadowed(76, 215, "ID: 24161099", c_cyan, c_black, 1);
    SDL_UpdateTexture(texture, NULL, framebuffer, TFT_WIDTH * sizeof(uint16_t));
    SDL_RenderClear(renderer); SDL_RenderCopy(renderer, texture, NULL, NULL); SDL_RenderPresent(renderer);
    SDL_Delay(3000);

    Vector3D base_verts[VERTEX_COUNT] = {
        {-1,-1,-1,1}, {1,-1,-1,1}, {1,1,-1,1}, {-1,1,-1,1},
        {-1,-1,1,1}, {1,-1,1,1}, {1,1,1,1}, {-1,1,1,1},
        {0,0,0,1}, {1.3f,0,0,1}, {0,1.3f,0,1}, {0,0,1.3f,1}
    };
    Vector3D scaled[VERTEX_COUNT], trans[VERTEX_COUNT];
    Point2D proj[VERTEX_COUNT];
    const Edge edges[EDGE_COUNT] = {{0,1},{1,2},{2,3},{3,0}, {4,5},{5,6},{6,7},{7,4}, {0,4},{1,5},{2,6},{3,7}};

    float rot_x = 0.0f, rot_y = 0.0f, rot_z = 0.0f, matrix[9];
    float t_cpu = 0.0f, t_temp = 40.0f, t_ram = 0.0f;
    float s_cpu = 0.0f, s_temp = 40.0f, s_ram = 0.0f;
    Uint32 last_poll = 0; const float EMA = 0.05f; 

    bool running = true; SDL_Event event;
    const uint16_t bg = RGB_To_RGB565(5, 8, 15), grid = RGB_To_RGB565(15, 25, 40);
    char buf[64];

    Uint64 prev_ctr = SDL_GetPerformanceCounter();
    float p_freq = (float)SDL_GetPerformanceFrequency();

    while (running) {
        Uint64 cur_ctr = SDL_GetPerformanceCounter();
        float dt = (float)(cur_ctr - prev_ctr) / p_freq; 
        prev_ctr = cur_ctr;
        if (dt > 0.05f) dt = 0.016f; 
        
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }
        
        Uint32 ticks = SDL_GetTicks();
        if (ticks - last_poll > 500) {
            t_cpu = Read_CPULoad(); 
            t_temp = Read_CPUTemperature(); 
            t_ram = Read_RAMUsage();
            last_poll = ticks;
        }
        s_cpu += EMA * (t_cpu - s_cpu); 
        s_temp += EMA * (t_temp - s_temp); 
        s_ram += EMA * (t_ram - s_ram);

        #pragma GCC unroll 8
        for (int i = 0; i < TFT_WIDTH * TFT_HEIGHT; i++) {
            framebuffer[i] = bg;
        }
        
        for (int x = 10; x < TFT_WIDTH; x += 15) {
            for (int y = 10; y < TFT_HEIGHT; y += 15) {
                Draw_Pixel(x, y, grid);
            }
        }

        float rot_spd = 0.4f + (s_cpu * 4.0f);
        rot_x += rot_spd * dt; 
        rot_y += rot_spd * 1.3f * dt; 
        rot_z += rot_spd * 0.4f * dt;

        float scale = 0.8f + ((s_temp - 30.0f) / 100.0f);
        for (int i = 0; i < VERTEX_COUNT; ++i) {
            scaled[i].x = base_verts[i].x * scale; 
            scaled[i].y = base_verts[i].y * scale;
            scaled[i].z = base_verts[i].z * scale; 
            scaled[i].w = 1.0f;
        }

        float norm = (s_temp - 40.0f) / 45.0f;
        if (norm < 0.0f) norm = 0.0f; 
        if (norm > 1.0f) norm = 1.0f;
        
        uint8_t r = (uint8_t)(norm * 255.0f);
        uint8_t g = (uint8_t)((1.0f - norm) * 255.0f);
        uint8_t b = (uint8_t)((1.0f - norm) * 255.0f + (norm * 50.0f));

        Graphics3D_ComputeRotationMatrix(matrix, rot_x, rot_y, rot_z);
        Graphics3D_RotateVertices(scaled, trans, matrix, VERTEX_COUNT);
        Graphics3D_ProjectVertices(trans, proj, VERTEX_COUNT, TFT_WIDTH, TFT_HEIGHT, 120.0f, 3.0f);

        Draw_Instrument_Dial(TFT_WIDTH/2, TFT_HEIGHT/2, 60, RGB_To_RGB565(r/3, g/3, b/3), c_cyan);

        for (int i = 0; i < EDGE_COUNT; ++i) {
            float intensity = 1.0f - (((trans[edges[i].v1].z + trans[edges[i].v2].z) * 0.5f + 1.5f) / 3.0f);
            if (intensity < 0.15f) intensity = 0.15f; 
            if (intensity > 1.0f) intensity = 1.0f;
            
            Graphics3D_DrawLine(framebuffer, TFT_WIDTH, TFT_HEIGHT, 
                                proj[edges[i].v1].x, proj[edges[i].v1].y, 
                                proj[edges[i].v2].x, proj[edges[i].v2].y, 
                                RGB_To_RGB565((uint8_t)(r*intensity), (uint8_t)(g*intensity), (uint8_t)(b*intensity)));
        }

        Point2D c = proj[8];
        Graphics3D_DrawLine(framebuffer, TFT_WIDTH, TFT_HEIGHT, c.x, c.y, proj[9].x, proj[9].y, RGB_To_RGB565(255, 30, 80));
        Graphics3D_DrawLine(framebuffer, TFT_WIDTH, TFT_HEIGHT, c.x, c.y, proj[10].x, proj[10].y, RGB_To_RGB565(30, 255, 80));
        Graphics3D_DrawLine(framebuffer, TFT_WIDTH, TFT_HEIGHT, c.x, c.y, proj[11].x, proj[11].y, RGB_To_RGB565(50, 100, 255));
        Draw_Rect(c.x - 1, c.y - 1, 3, 3, c_white);

        int l = 15, t = 2;
        Draw_Rect(5,5,l,t,c_mag); Draw_Rect(5,5,t,l,c_mag);
        Draw_Rect(TFT_WIDTH-5-l,5,l,t,c_mag); Draw_Rect(TFT_WIDTH-5-t,5,t,l,c_mag);
        Draw_Rect(5,TFT_HEIGHT-5-t,l,t,c_mag); Draw_Rect(5,TFT_HEIGHT-5-l,t,l,c_mag);
        Draw_Rect(TFT_WIDTH-5-l,TFT_HEIGHT-5-t,l,t,c_mag); Draw_Rect(TFT_WIDTH-5-t,TFT_HEIGHT-5-l,t,l,c_mag);

        uint16_t bar = RGB_To_RGB565(r, g, b);
        snprintf(buf, sizeof(buf), "CPU:%3.0f%%", s_cpu * 100.0f);
        Draw_Text_Shadowed(10, 10, buf, c_white, c_black, 1);
        Draw_Rect(10, 20, (int)(s_cpu * 60.0f), 3, bar);

        snprintf(buf, sizeof(buf), "RAM:%3.0f%%", s_ram * 100.0f);
        Draw_Text_Shadowed(TFT_WIDTH - 75, 10, buf, c_white, c_black, 1);
        Draw_Rect(TFT_WIDTH - 75, 20, (int)(s_ram * 60.0f), 3, c_cyan);

        snprintf(buf, sizeof(buf), "TMP:%3.1fC", s_temp);
        Draw_Text_Shadowed(10, TFT_HEIGHT - 25, buf, bar, c_black, 1);

        /* Initialize variables to 0 to avert [-Werror=maybe-uninitialized] */
        int hh = 0, mm = 0, ss = 0; 
        Read_Uptime(&hh, &mm, &ss);
        snprintf(buf, sizeof(buf), "UPT:%02d:%02d:%02d", hh, mm, ss);
        Draw_Text_Shadowed(10, TFT_HEIGHT - 12, buf, c_white, c_black, 1);

        snprintf(buf, sizeof(buf), "TSK:%d", Read_TotalTasks());
        Draw_Text_Shadowed(TFT_WIDTH - 60, TFT_HEIGHT - 12, buf, c_white, c_black, 1);

        SDL_UpdateTexture(texture, NULL, framebuffer, TFT_WIDTH * sizeof(uint16_t));
        SDL_RenderClear(renderer); SDL_RenderCopy(renderer, texture, NULL, NULL); 
        SDL_RenderPresent(renderer);
    }
    SDL_DestroyTexture(texture); SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
    return 0;
}
