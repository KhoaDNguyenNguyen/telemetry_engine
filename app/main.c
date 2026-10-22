#define _DEFAULT_SOURCE

#include "hal_rpi.h"
#include "bcm2837_hw.h"
#include "st7789_logic.h"
#include "graphics_3d.h"
#include "boot_logo.h"
#include "assets.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <errno.h>

static int system_mount_vfs(void) {
    if (mkdir("/dev", 0755) < 0 && errno != EEXIST) return -1;
    if (mount("devtmpfs", "/dev", "devtmpfs", 0, NULL) < 0) return -1;
    if (mkdir("/sys", 0755) < 0 && errno != EEXIST) return -1;
    if (mount("sysfs", "/sys", "sysfs", 0, NULL) < 0) return -1;
    if (mkdir("/proc", 0755) < 0 && errno != EEXIST) return -1;
    if (mount("proc", "/proc", "proc", 0, NULL) < 0) return -1;
    return 0;
}

#define TFT_WIDTH  240
#define TFT_HEIGHT 240
#define PIXEL_COUNT (TFT_WIDTH * TFT_HEIGHT)

#define BTN_JUMP  ((uint8_t)16)
#define BTN_DUCK  ((uint8_t)20)
#define BTN_START ((uint8_t)21)

#define I2C_SDA ((uint8_t)2)
#define I2C_SCL ((uint8_t)3)
#define LCD_ADDR 0x27

static inline void i2c_delay(void) { 
    for(volatile int i = 0; i < 2000; i++) __asm__ volatile("nop");
}

static void i2c_set_scl(uint8_t state) {
    if (state) {
        hal_gpio_set_function(I2C_SCL, GPIO_INPUT);
    } else {
        hal_gpio_write(I2C_SCL, 0);
        hal_gpio_set_function(I2C_SCL, GPIO_OUTPUT);
    }
    i2c_delay();
}

static void i2c_set_sda(uint8_t state) {
    if (state) {
        hal_gpio_set_function(I2C_SDA, GPIO_INPUT);
    } else {
        hal_gpio_write(I2C_SDA, 0);
        hal_gpio_set_function(I2C_SDA, GPIO_OUTPUT);
    }
    i2c_delay();
}

static void i2c_start(void) {
    i2c_set_sda(1); i2c_set_scl(1);
    i2c_set_sda(0); i2c_set_scl(0);
}

static void i2c_stop(void) {
    i2c_set_sda(0); i2c_set_scl(1); i2c_set_sda(1);
}

static void i2c_write_byte(uint8_t data) {
    for (int i = 0; i < 8; i++) {
        i2c_set_sda((data & 0x80U) ? 1 : 0);
        data <<= 1;
        i2c_set_scl(1); i2c_set_scl(0);
    }
    i2c_set_sda(1); i2c_set_scl(1); i2c_set_scl(0);
}

static void lcd_send(uint8_t data, uint8_t mode) {
    uint8_t high = (uint8_t)(data & 0xF0U);
    uint8_t low = (uint8_t)((data << 4) & 0xF0U);
    uint8_t bl = 0x08U;
    i2c_start();
    i2c_write_byte((uint8_t)(LCD_ADDR << 1)); 
    i2c_write_byte((uint8_t)(high | mode | bl | 0x04U)); i2c_delay();
    i2c_write_byte((uint8_t)(high | mode | bl)); i2c_delay();
    i2c_write_byte((uint8_t)(low | mode | bl | 0x04U)); i2c_delay();
    i2c_write_byte((uint8_t)(low | mode | bl)); i2c_delay();
    i2c_stop();
}

static void lcd_send_cmd_raw(uint8_t data) {
    uint8_t bl = 0x08U;
    i2c_start();
    i2c_write_byte((uint8_t)(LCD_ADDR << 1));
    i2c_write_byte((uint8_t)(data | bl | 0x04U)); i2c_delay();
    i2c_write_byte((uint8_t)(data | bl)); i2c_delay();
    i2c_stop();
}

static void lcd_init(void) {
    hal_gpio_write(I2C_SDA, 0);
    hal_gpio_write(I2C_SCL, 0);
    usleep(50000);
    lcd_send_cmd_raw(0x30U); usleep(5000);
    lcd_send_cmd_raw(0x30U); usleep(200);
    lcd_send_cmd_raw(0x30U); usleep(200);
    lcd_send_cmd_raw(0x20U); usleep(200);
    lcd_send(0x28, 0); usleep(50);
    lcd_send(0x0C, 0); usleep(50);
    lcd_send(0x01, 0); usleep(2000);
    lcd_send(0x06, 0); usleep(50);
}

static void lcd_print(const char *str, uint8_t line) {
    lcd_send((uint8_t)((line == 1) ? 0x80U : 0xC0U), 0);
    while (*str) {
        lcd_send((uint8_t)(*str++), 1);
    }
}

static uint16_t frame_buffer[PIXEL_COUNT];

static inline uint16_t rgb_to_rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r & 0xF8U) << 8) | ((g & 0xFCU) << 3) | (b >> 3));
}

static inline void draw_pixel(int32_t x, int32_t y, uint16_t color) {
    if (x >= 0 && x < TFT_WIDTH && y >= 0 && y < TFT_HEIGHT) frame_buffer[(uint32_t)y * TFT_WIDTH + (uint32_t)x] = color;
}

static void draw_bitmap(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t *b) {
    for (int32_t i = 0; i < h; i++) {
        for (int32_t j = 0; j < w; j++) {
            if (b[i * w + j] != 0x0000) draw_pixel(x + j, y + i, b[i * w + j]);
        }
    }
}

static void draw_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t color) {
    for (int32_t i = 0; i < h; i++)
        for (int32_t j = 0; j < w; j++)
            draw_pixel(x + j, y + i, color);
}

static void draw_text_shadowed(int16_t x, int16_t y, const char *text, uint16_t color, uint16_t shadow, uint8_t scale) {
    Graphics3D_DrawString(frame_buffer, TFT_WIDTH, TFT_HEIGHT, (int16_t)(x + 1), (int16_t)(y + 1), text, shadow, scale);
    Graphics3D_DrawString(frame_buffer, TFT_WIDTH, TFT_HEIGHT, x, y, text, color, scale);
}

static int is_btn_pressed(uint8_t pin, int *last_state) {
    int current_state = (hal_gpio_get_level(pin) == 0);
    int is_trigger = (current_state && !(*last_state));
    *last_state = current_state;
    return is_trigger;
}

int main(void) {
    if (system_mount_vfs() < 0) while(1);

    int mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (mem_fd < 0) while(1);
    if (hal_rpi_init(mem_fd) < 0) { close(mem_fd); while(1); }
    close(mem_fd);

    hal_gpio_set_function((uint8_t)8, GPIO_ALT0); hal_gpio_set_function((uint8_t)9, GPIO_ALT0);
    hal_gpio_set_function((uint8_t)10, GPIO_ALT0); hal_gpio_set_function((uint8_t)11, GPIO_ALT0);
    hal_gpio_set_function((uint8_t)24, GPIO_OUTPUT); hal_gpio_set_function((uint8_t)25, GPIO_OUTPUT);

    hal_gpio_set_function(BTN_JUMP, GPIO_INPUT); hal_gpio_set_pullup(BTN_JUMP);
    hal_gpio_set_function(BTN_DUCK, GPIO_INPUT); hal_gpio_set_pullup(BTN_DUCK);
    hal_gpio_set_function(BTN_START, GPIO_INPUT); hal_gpio_set_pullup(BTN_START);

    const spi_driver_ops_t *spi_ops = hal_rpi_get_spi_ops();
    st7789_init_sequence(spi_ops);

    const uint16_t c_white  = rgb_to_rgb565(255, 255, 255);
    const uint16_t c_black  = rgb_to_rgb565(0, 0, 0);
    const uint16_t c_red    = rgb_to_rgb565(255, 30, 30);
    const uint16_t c_yellow = rgb_to_rgb565(255, 255, 0);
    const uint16_t bg       = rgb_to_rgb565(135, 206, 235);

    for (int32_t i = 0; i < PIXEL_COUNT; i++) frame_buffer[i] = BOOT_LOGO[i];
    
    draw_text_shadowed(40, 195, "Assc Prof:", c_yellow, c_black, 2);
    draw_text_shadowed(0, 215, "Truong Ngoc Son", c_white, c_black, 2);
    st7789_flush_buffer(spi_ops, frame_buffer, TFT_WIDTH, TFT_HEIGHT);
    
    lcd_init();
    lcd_print("DINO RUNNER     ", 1);
    lcd_print("Initializing... ", 2);
    for (int k = 0; k < 80; k++) {
        st7789_flush_buffer(spi_ops, frame_buffer, TFT_WIDTH, TFT_HEIGHT);
        usleep(20000);
    }

    int game_state = 0; 
    float dino_y = 180.0f, dino_vy = 0.0f;
    int dino_w = 48, dino_h = 48;
    float obs_x = 240.0f;
    int obs_w = 32, obs_h = 48, obs_type = 0;
    float speed = 4.0f;
    int score = 0, high_score = 0;
    
    int btn_jump_last = 0, btn_start_last = 0, btn_duck_last = 0;
    int lcd_page = 0;
    int last_game_state = -1, last_score = -1, last_lcd_page = -1;

    char lcd_buf[32];
    char tft_buf[64];

    while (1) {
        int trigger_jump = is_btn_pressed(BTN_JUMP, &btn_jump_last);
        int trigger_start = is_btn_pressed(BTN_START, &btn_start_last);
        int trigger_duck = is_btn_pressed(BTN_DUCK, &btn_duck_last);
        int is_ducking = (hal_gpio_get_level(BTN_DUCK) == 0);

        if (trigger_duck) {
            lcd_page = (lcd_page + 1) % 3;
        }

        if (game_state == 0) {
            if (trigger_start || trigger_jump) {
                game_state = 1;
                score = 0; speed = 4.0f; dino_y = 180.0f; obs_x = 240.0f;
                lcd_page = 0;
            }
        } else if (game_state == 1) {
            dino_h = (is_ducking && dino_y == 180.0f) ? 24 : 48;
            if ((trigger_jump || trigger_start) && dino_y == 180.0f) dino_vy = -10.5f;

            dino_y += dino_vy;
            dino_vy += 0.7f;
            if (dino_y >= 180.0f) { dino_y = 180.0f; dino_vy = 0.0f; }

            obs_x -= speed;
            if (obs_x < -50.0f) {
                obs_x = 240.0f; score += 10;
                speed += 0.3f; if (speed > 16.0f) speed = 16.0f;
                obs_type = (score % 30 == 0) ? 1 : 0; 
                obs_h = obs_type ? 24 : 48; obs_w = obs_type ? 40 : 32;
            }

            float pad_x = 8.0f;
            float pad_y = 8.0f;
            float o_pad_x = 6.0f;
            float o_pad_y = 6.0f;

            float d_l = 30.0f + pad_x;
            float d_r = 30.0f + (float)dino_w - pad_x;
            float d_t = dino_y - (float)dino_h + pad_y;
            float d_b = dino_y - pad_y;
            
            float o_bot = (obs_type == 1) ? 155.0f : 180.0f;
            float o_top = o_bot - (float)obs_h;
            
            float o_l = obs_x + o_pad_x;
            float o_r = obs_x + (float)obs_w - o_pad_x;
            float o_t = o_top + o_pad_y;
            float o_b = o_bot - o_pad_y;

            if (d_r > o_l && d_l < o_r && d_b > o_t && d_t < o_b) {
                game_state = 2;
                if (score > high_score) high_score = score;
            }
        } else if (game_state == 2) {
            if (trigger_start || trigger_jump) {
                game_state = 0;
            }
        }

        if (game_state != last_game_state || score != last_score || lcd_page != last_lcd_page) {
            last_game_state = game_state;
            last_score = score;
            last_lcd_page = lcd_page;

            if (lcd_page == 0) {
                if (game_state == 0) {
                    lcd_print("DINO RUNNER     ", 1);
                    lcd_print("PRESS START     ", 2);
                } else if (game_state == 1) {
                    lcd_print("DINO RUNNER     ", 1);
                    snprintf(lcd_buf, sizeof(lcd_buf), "SCORE: %05d    ", score);
                    lcd_print(lcd_buf, 2);
                } else {
                    lcd_print("GAME OVER       ", 1);
                    snprintf(lcd_buf, sizeof(lcd_buf), "SCORE: %05d    ", score);
                    lcd_print(lcd_buf, 2);
                }
            } else if (lcd_page == 1) {
                lcd_print("Nguyen Dinh Khoi", 1);
                lcd_print("24161101        ", 2);
            } else if (lcd_page == 2) {
                lcd_print("Le Vu Thanh Danh", 1);
                lcd_print("24161068        ", 2);
            }
        }

        for (int32_t i = 0; i < PIXEL_COUNT; i++) frame_buffer[i] = bg;
        draw_rect(0, 180, 240, 2, c_black);
        
        draw_bitmap(30, (int32_t)dino_y - dino_h, dino_w, dino_h, FACE_SPRITE);
        int o_y = (obs_type == 1) ? (155 - obs_h) : (180 - obs_h);
        if (obs_type == 1) draw_bitmap((int32_t)obs_x, o_y, obs_w, obs_h, PLANE_SPRITE); 
        else draw_bitmap((int32_t)obs_x, o_y, obs_w, obs_h, CACTUS_SPRITE);

        snprintf(tft_buf, sizeof(tft_buf), "HI: %05d  SCR: %05d", high_score, score);
        draw_text_shadowed(10, 10, tft_buf, c_white, c_black, 1);
        
        if (game_state == 0) draw_text_shadowed(45, 100, "PRESS START", c_yellow, c_black, 1);
        if (game_state == 2) draw_text_shadowed(60, 100, "GAME OVER", c_red, c_black, 2);

        st7789_flush_buffer(spi_ops, frame_buffer, TFT_WIDTH, TFT_HEIGHT);
    }
    return 0;
}
