#ifndef HAL_SPI_RPI_H
#define HAL_SPI_RPI_H

#include "../hal/hal_spi.h"
#include <linux/platform_device.h>

/**
 * @brief Initializes the MMIO mapping for the Raspberry Pi SPI controller.
 * 
 * @param pdev Pointer to the platform device structure.
 * @return spi_driver_ops_t* Pointer to the initialized operations structure, or NULL on failure.
 */
const spi_driver_ops_t* RPi_SPI_Init(struct platform_device *pdev);

/**
 * @brief Releases the allocated MMIO regions.
 */
void RPi_SPI_Deinit(void);

#endif /* HAL_SPI_RPI_H */
