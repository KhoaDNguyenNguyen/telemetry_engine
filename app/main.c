#define _DEFAULT_SOURCE

#include "hal_rpi.h"
#include "bcm2837_hw.h"
#include "st7789_logic.h"
#include "graphics_3d.h"
#include "boot_logo.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

#define TFT_WIDTH  240
#define TFT_HEIGHT 240
#define PIXEL_COUNT (TFT_WIDTH * TFT_HEIGHT)
#define VERTEX_COUNT 12
#define EDGE_COUNT 12

static uint16_t frame_buffer[PIXEL_COUNT];

/* --- SYSTEM LIFECYCLE --- */

static int system_mount_vfs(void) {
    if (mkdir("/dev", 0755) < 0 && errno != EEXIST) return -1;
    if (mount("devtmpfs", "/dev", "devtmpfs", 0, NULL) < 0) return -1;
    
    if (mkdir("/sys", 0755) < 0 && errno != EEXIST) return -1;
    if (mount("sysfs", "/sys", "sysfs", 0, NULL) < 0) return -1;
    
    if (mkdir("/proc", 0755) < 0 && errno != EEXIST) return -1;
    if (mount("proc", "/proc", "proc", 0, NULL) < 0) return -1;
    
    return 0;
}

static void system_panic(void) {
    while (1) { usleep(1000000); }
}

/* --- BACKGROUND STRESS SIMULATOR --- */

/**
 * @brief Forks a background sequencer to periodically stress the CPU.
 * Simulates engine revving (thermal load) by spawning and killing infinite-loop workers.
 */
static void load_sim_start_background(void) {
    pid_t sequencer_pid = fork();
    if (sequencer_pid == 0) {
        while (1) {
            pid_t workers[4];
            
            /* Phase 1: High sustained load (Revving up) */
            for (int32_t i = 0; i < 4; i++) {
                if ((workers[i] = fork()) == 0) {
                    while(1); /* Child process: Burn CPU cycles */
                }
            }
            sleep(40);
            
            /* Phase 2: Complete idle (Cooling down) */
            for (int32_t i = 0; i < 4; i++) kill(workers[i], SIGKILL);
            for (int32_t i = 0; i < 4; i++) waitpid(workers[i], NULL, 0);
            sleep(15);
            
            /* Phase 3: Rapid burst load (Pulsing) */
            for (int32_t r = 0; r < 4; r++) {
                for (int32_t i = 0; i < 4; i++) {
                    if ((workers[i] = fork()) == 0) {
                        while(1);
                    }
                }
                sleep(5);
                for (int32_t i = 0; i < 4; i++) kill(workers[i], SIGKILL);
                for (int32_t i = 0; i < 4; i++) waitpid(workers[i], NULL, 0);
                sleep(2);
            }
        }
        exit(0);
    }
}

/* --- SYSTEM TELEMETRY PROBES --- */

static float read_cpu_load(void) {
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

static float read_cpu_temp(void) {
    char path[64]; int max_temp = 0;
    for (int32_t i = 0; i < 5; i++) {
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

static float read_ram_usage(void) {
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

static void read_uptime(int *hours, int *mins, int *secs) {
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

static int read_total_tasks(void) {
    FILE *fd = fopen("/proc/loadavg", "r");
    int running = 0, total = 0;
    if (fd) {
        if (fscanf(fd, "%*f %*f %*f %d/%d", &running, &total) != 2) { total = 0; }
        fclose(fd);
    }
    return total;
}

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
    int32_t x = 0; int32_t y = r; int32_t d = 3 - 2 * r;
    while (y >= x) {
        draw_pixel(xc+x, yc+y, ring_color); draw_pixel(xc-x, yc+y, ring_color);
        draw_pixel(xc+x, yc-y, ring_color); draw_pixel(xc-x, yc-y, ring_color);
        draw_pixel(xc+y, yc+x, ring_color); draw_pixel(xc-y, yc+x, ring_color);
        draw_pixel(xc+y, yc-x, ring_color); draw_pixel(xc-y, yc-x, ring_color);
        x++; 
        if (d > 0) { y--; d = d + 4 * (x - y) + 10; } 
        else { d = d + 4 * x + 6; }
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
    if (system_mount_vfs() < 0) system_panic();

    /* Initialize background thermal load simulator */
    load_sim_start_background();

    int mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (mem_fd < 0) system_panic();

    if (hal_rpi_init(mem_fd) < 0) {
        close(mem_fd);
        system_panic();
    }
    close(mem_fd);

    hal_gpio_set_function(8, GPIO_ALT0); hal_gpio_set_function(9, GPIO_ALT0);
    hal_gpio_set_function(10, GPIO_ALT0); hal_gpio_set_function(11, GPIO_ALT0);
    hal_gpio_set_function(24, GPIO_OUTPUT); hal_gpio_set_function(25, GPIO_OUTPUT);

    const spi_driver_ops_t *spi_ops = hal_rpi_get_spi_ops();
    st7789_init_sequence(spi_ops);

    const uint16_t c_white = rgb_to_rgb565(255, 255, 255);
    const uint16_t c_black = rgb_to_rgb565(0, 0, 0);
    const uint16_t c_cyan  = rgb_to_rgb565(0, 255, 255);
    const uint16_t c_mag   = rgb_to_rgb565(255, 0, 255);
    const uint16_t bg      = rgb_to_rgb565(5, 8, 15);
    const uint16_t grid    = rgb_to_rgb565(15, 25, 40);

    /* Phase 1: Boot Logo Overlay */
    memcpy(frame_buffer, BOOT_LOGO, PIXEL_COUNT * sizeof(uint16_t));
    draw_text_shadowed(44, 200, "Dang-Khoa N. Nguyen", c_white, c_black, 1);
    draw_text_shadowed(76, 215, "ID: 24161099", c_cyan, c_black, 1);
    st7789_flush_buffer(spi_ops, frame_buffer, TFT_WIDTH, TFT_HEIGHT);
    usleep(3000000);

    /* Phase 2: Telemetry Render Engine setup */
    Vector3D base_verts[VERTEX_COUNT] = {
        {-1.0f,-1.0f,-1.0f,1.0f}, {1.0f,-1.0f,-1.0f,1.0f}, {1.0f,1.0f,-1.0f,1.0f}, {-1.0f,1.0f,-1.0f,1.0f},
        {-1.0f,-1.0f,1.0f,1.0f},  {1.0f,-1.0f,1.0f,1.0f},  {1.0f,1.0f,1.0f,1.0f},  {-1.0f,1.0f,1.0f,1.0f},
        {0.0f,0.0f,0.0f,1.0f},    {1.3f,0.0f,0.0f,1.0f},   {0.0f,1.3f,0.0f,1.0f},  {0.0f,0.0f,1.3f,1.0f}
    };
    Vector3D scaled[VERTEX_COUNT], trans[VERTEX_COUNT]; Point2D proj[VERTEX_COUNT];
    const Edge edges[EDGE_COUNT] = {{0,1},{1,2},{2,3},{3,0}, {4,5},{5,6},{6,7},{7,4}, {0,4},{1,5},{2,6},{3,7}};

    float rot_x = 0.0f, rot_y = 0.0f, rot_z = 0.0f, matrix[9];
    float t_cpu = 0.0f, t_temp = 40.0f, t_ram = 0.0f;
    float s_cpu = 0.0f, s_temp = 40.0f, s_ram = 0.0f;
    const float EMA = 0.05f; 
    const float dt = 0.033f;
    
    char buf[64];
    uint32_t frame_counter = 0;

    /* Tell QEMU/Serial that UI loop is starting */
    printf("[PID 1] Hardware initialized. Entering Render Loop...\n");
    fflush(stdout);

    while (1) {
        if (frame_counter % 15 == 0) {
            t_cpu = read_cpu_load(); 
            t_temp = read_cpu_temp(); 
            t_ram = read_ram_usage();
        }
        frame_counter++;

        s_cpu += EMA * (t_cpu - s_cpu); 
        s_temp += EMA * (t_temp - s_temp); 
        s_ram += EMA * (t_ram - s_ram);

        for (int32_t i = 0; i < PIXEL_COUNT; i++) frame_buffer[i] = bg;
        for (int32_t x = 10; x < TFT_WIDTH; x += 15) {
            for (int32_t y = 10; y < TFT_HEIGHT; y += 15) {
                draw_pixel(x, y, grid);
            }
        }

        float rot_spd = 0.4f + (s_cpu * 4.0f);
        rot_x += rot_spd * dt; rot_y += rot_spd * 1.3f * dt; rot_z += rot_spd * 0.4f * dt;

        float scale = 0.8f + ((s_temp - 30.0f) / 100.0f);
        for (int32_t i = 0; i < VERTEX_COUNT; ++i) {
            scaled[i].x = base_verts[i].x * scale; scaled[i].y = base_verts[i].y * scale;
            scaled[i].z = base_verts[i].z * scale; scaled[i].w = 1.0f;
        }

        /* 
         * Thermal calibration: Normalized for [40.0, 57.0] Celsius range 
         * norm = 0.0 at 40C (Cyan/Blue), norm = 1.0 at 57C (Red/Magenta)
         */
        float norm = (s_temp - 40.0f) / 17.0f;
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
            Graphics3D_DrawLine(frame_buffer, TFT_WIDTH, TFT_HEIGHT, proj[edges[i].v1].x, proj[edges[i].v1].y, proj[edges[i].v2].x, proj[edges[i].v2].y, edge_color);
        }

        Point2D c = proj[8];
        Graphics3D_DrawLine(frame_buffer, TFT_WIDTH, TFT_HEIGHT, c.x, c.y, proj[9].x, proj[9].y, rgb_to_rgb565(255, 30, 80));
        Graphics3D_DrawLine(frame_buffer, TFT_WIDTH, TFT_HEIGHT, c.x, c.y, proj[10].x, proj[10].y, rgb_to_rgb565(30, 255, 80));
        Graphics3D_DrawLine(frame_buffer, TFT_WIDTH, TFT_HEIGHT, c.x, c.y, proj[11].x, proj[11].y, rgb_to_rgb565(50, 100, 255));
        draw_rect(c.x - 1, c.y - 1, 3, 3, c_white);

        int32_t l = 15, t = 2;
        draw_rect(5,5,l,t,c_mag); draw_rect(5,5,t,l,c_mag);
        draw_rect(TFT_WIDTH-5-l,5,l,t,c_mag); draw_rect(TFT_WIDTH-5-t,5,t,l,c_mag);
        draw_rect(5,TFT_HEIGHT-5-t,l,t,c_mag); draw_rect(5,TFT_HEIGHT-5-l,t,l,c_mag);
        draw_rect(TFT_WIDTH-5-l,TFT_HEIGHT-5-t,l,t,c_mag); draw_rect(TFT_WIDTH-5-t,TFT_HEIGHT-5-l,t,l,c_mag);

        uint16_t bar_color = rgb_to_rgb565(r, g, b);
        const int32_t pad_x = 24;

        snprintf(buf, sizeof(buf), "CPU:%3.0f%%", s_cpu * 100.0f);
        draw_text_shadowed(pad_x, 10, buf, c_white, c_black, 1);
        draw_rect(pad_x, 20, (int32_t)(s_cpu * 60.0f), 3, bar_color);

        snprintf(buf, sizeof(buf), "RAM:%3.0f%%", s_ram * 100.0f);
        draw_text_shadowed(TFT_WIDTH - 85, 10, buf, c_white, c_black, 1);
        draw_rect(TFT_WIDTH - 85, 20, (int32_t)(s_ram * 60.0f), 3, c_cyan);

        snprintf(buf, sizeof(buf), "TMP:%3.1fC", s_temp);
        draw_text_shadowed(pad_x, TFT_HEIGHT - 30, buf, bar_color, c_black, 1);

        int hh = 0, mm = 0, ss = 0; read_uptime(&hh, &mm, &ss);
        snprintf(buf, sizeof(buf), "UPT:%02d:%02d:%02d", hh, mm, ss);
        draw_text_shadowed(pad_x, TFT_HEIGHT - 16, buf, c_white, c_black, 1);

        snprintf(buf, sizeof(buf), "TSK:%d", read_total_tasks());
        draw_text_shadowed(TFT_WIDTH - 75, TFT_HEIGHT - 16, buf, c_white, c_black, 1);

        st7789_flush_buffer(spi_ops, frame_buffer, TFT_WIDTH, TFT_HEIGHT);
    }
    return 0;
}