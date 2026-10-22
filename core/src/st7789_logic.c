#include "st7789_logic.h"
#include <stddef.h>

#define ST7789_SWRESET 0x01U
#define ST7789_SLPOUT  0x11U
#define ST7789_COLMOD  0x3AU
#define ST7789_MADCTL  0x36U
#define ST7789_INVON   0x21U
#define ST7789_NORON   0x13U
#define ST7789_DISPON  0x29U
#define ST7789_CASET   0x2AU
#define ST7789_RASET   0x2BU
#define ST7789_RAMWR   0x2CU

void st7789_init_sequence(const spi_driver_ops_t *ops) {
    if (!ops || !ops->write_command || !ops->write_data) return;
    if (ops->hard_reset) ops->hard_reset();
    ops->write_command(ST7789_SWRESET);
    if (ops->delay_ms) ops->delay_ms(150);
    ops->write_command(ST7789_SLPOUT);
    if (ops->delay_ms) ops->delay_ms(120); 
    ops->write_command(ST7789_COLMOD);
    ops->write_data(0x55U); 
    ops->write_command(ST7789_MADCTL);
    ops->write_data(0x00U); 
    ops->write_command(ST7789_INVON);
    ops->write_command(ST7789_NORON);
    if (ops->delay_ms) ops->delay_ms(10);
    ops->write_command(ST7789_DISPON);
    if (ops->delay_ms) ops->delay_ms(100);
}

void st7789_flush_buffer(const spi_driver_ops_t *ops, const uint16_t *framebuffer, uint16_t width, uint16_t height) {
    if (!ops || !ops->write_command || !ops->write_buffer || !framebuffer) return;
    ops->write_command(ST7789_CASET);
    ops->write_data(0x00U);
    ops->write_data(0x00U);
    ops->write_data((uint8_t)((width - 1) >> 8));
    ops->write_data((uint8_t)((width - 1) & 0xFFU)); 
    ops->write_command(ST7789_RASET);
    ops->write_data(0x00U);
    ops->write_data(0x00U);
    ops->write_data((uint8_t)((height - 1) >> 8));
    ops->write_data((uint8_t)((height - 1) & 0xFFU)); 
    ops->write_command(ST7789_RAMWR);
    ops->write_buffer(framebuffer, (uint32_t)width * height);
}
