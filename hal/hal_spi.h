#ifndef HAL_SPI_H
#define HAL_SPI_H

#include <stdint.h>

/**
 * @brief Abstract interface for Hardware SPI operations.
 * 
 * Adheres to the Dependency Inversion Principle. The ST7789 logic layer 
 * will utilize these function pointers rather than direct memory-mapped I/O.
 */
typedef struct {
    void (*write_command)(uint8_t cmd);
    void (*write_data)(uint8_t data);
    void (*hard_reset)(void);
    void (*delay_ms)(uint32_t ms);
} spi_driver_ops_t;

#endif /* HAL_SPI_H */
