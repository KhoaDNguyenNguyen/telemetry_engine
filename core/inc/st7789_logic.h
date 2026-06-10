#ifndef ST7789_LOGIC_H
#define ST7789_LOGIC_H

#include "hal_spi.h"
#include <stdint.h>

/**
 * @brief Executes the hex instruction sequence to initialize the ST7789 controller.
 * 
 * @param spi_ops Pointer to the hardware-specific SPI operations implementation.
 */
void ST7789_InitSequence(const spi_driver_ops_t *spi_ops);

/**
 * @brief Transmits the entire geometry framebuffer to the ST7789 GRAM.
 * 
 * @param spi_ops Pointer to the hardware-specific SPI operations implementation.
 * @param framebuffer Pointer to the RGB565 memory buffer.
 * @param pixel_count Total number of pixels to transmit.
 */
void ST7789_FlushBuffer(const spi_driver_ops_t *spi_ops, const uint16_t *framebuffer, uint32_t pixel_count);

#endif /* ST7789_LOGIC_H */
