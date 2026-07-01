#ifndef HAL_RPI_H
#define HAL_RPI_H

#include "hal_spi.h"

/**
 * @brief Initializes memory-mapped I/O for GPIO and SPI peripherals.
 * @param mem_fd File descriptor for /dev/mem.
 * @return 0 on success, negative value on failure.
 */
int hal_rpi_init(int mem_fd);

/**
 * @brief Configures the pin multiplexer for a specific GPIO.
 */
void hal_gpio_set_function(uint8_t pin, uint8_t mode);

/**
 * @brief Returns the SPI operations interface for the BCM2837.
 */
const spi_driver_ops_t* hal_rpi_get_spi_ops(void);

#endif /* HAL_RPI_H */