#ifndef ST7789_LOGIC_H
#define ST7789_LOGIC_H

#include "hal_spi.h"

/**
 * @brief Executes the initialization sequence for the ST7789 controller.
 * @param ops Pointer to hardware SPI operations.
 */
void st7789_init_sequence(const spi_driver_ops_t *ops);

/**
 * @brief Transmits a contiguous RGB565 framebuffer to the controller SRAM.
 * @param ops Pointer to hardware SPI operations.
 * @param framebuffer Pointer to RGB565 memory buffer.
 * @param width Display width.
 * @param height Display height.
 */
void st7789_flush_buffer(const spi_driver_ops_t *ops, const uint16_t *framebuffer, uint16_t width, uint16_t height);

#endif /* ST7789_LOGIC_H */