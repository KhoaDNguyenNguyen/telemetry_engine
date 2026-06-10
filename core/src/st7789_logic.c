#include "st7789_logic.h"
#include <stddef.h>

/* ST7789 Standard Command Definitions */
#define ST7789_SWRESET 0x01
#define ST7789_SLPOUT  0x11
#define ST7789_COLMOD  0x3A
#define ST7789_MADCTL  0x36
#define ST7789_INVON   0x21
#define ST7789_NORON   0x13
#define ST7789_DISPON  0x29
#define ST7789_CASET   0x2A
#define ST7789_RASET   0x2B
#define ST7789_RAMWR   0x2C

void ST7789_InitSequence(const spi_driver_ops_t *spi_ops) {
    if (!spi_ops || !spi_ops->write_command || !spi_ops->write_data) return;

    /* Hardware Reset Execution */
    if (spi_ops->hard_reset) {
        spi_ops->hard_reset();
        spi_ops->delay_ms(150);
    }

    /* Software Reset */
    spi_ops->write_command(ST7789_SWRESET);
    spi_ops->delay_ms(150);

    /* Sleep Out */
    spi_ops->write_command(ST7789_SLPOUT);
    spi_ops->delay_ms(10);

    /* Color Mode Configuration: 16-bit RGB565 */
    spi_ops->write_command(ST7789_COLMOD);
    spi_ops->write_data(0x55);

    /* Memory Data Access Control: Orientation and RGB/BGR order */
    spi_ops->write_command(ST7789_MADCTL);
    spi_ops->write_data(0x00); /* 0x00 sets top-to-bottom, left-to-right */

    /* Inversion On - Required for standard IPS panels utilizing ST7789 */
    spi_ops->write_command(ST7789_INVON);

    /* Normal Display Mode On */
    spi_ops->write_command(ST7789_NORON);
    spi_ops->delay_ms(10);

    /* Display On */
    spi_ops->write_command(ST7789_DISPON);
    spi_ops->delay_ms(10);
}

void ST7789_FlushBuffer(const spi_driver_ops_t *spi_ops, const uint16_t *framebuffer, uint32_t pixel_count) {
    if (!spi_ops || !spi_ops->write_command || !spi_ops->write_data || !framebuffer) return;

    /* Define Column Address Set (0 to 239) */
    spi_ops->write_command(ST7789_CASET);
    spi_ops->write_data(0x00);
    spi_ops->write_data(0x00);
    spi_ops->write_data(0x00);
    spi_ops->write_data(0xEF); /* 239 */

    /* Define Row Address Set (0 to 239) */
    spi_ops->write_command(ST7789_RASET);
    spi_ops->write_data(0x00);
    spi_ops->write_data(0x00);
    spi_ops->write_data(0x00);
    spi_ops->write_data(0xEF); /* 239 */

    /* Memory Write Initialization */
    spi_ops->write_command(ST7789_RAMWR);

    /* Transmit Framebuffer sequence. 
       Note: Most physical implementations will replace this loop with DMA. */
    for (uint32_t i = 0; i < pixel_count; ++i) {
        const uint16_t pixel = framebuffer[i];
        /* Endianness adjustment: ST7789 expects MSB first */
        spi_ops->write_data((uint8_t)(pixel >> 8));
        spi_ops->write_data((uint8_t)(pixel & 0xFF));
    }
}
