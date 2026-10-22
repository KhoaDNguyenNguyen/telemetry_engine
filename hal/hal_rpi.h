#ifndef HAL_RPI_H
#define HAL_RPI_H
#include "hal_spi.h"

int hal_rpi_init(int mem_fd);
void hal_gpio_set_function(uint8_t pin, uint8_t mode);
uint8_t hal_gpio_get_level(uint8_t pin);
void hal_gpio_write(uint8_t pin, uint8_t val);
void hal_gpio_set_pullup(uint8_t pin);
const spi_driver_ops_t* hal_rpi_get_spi_ops(void);

#endif
