#include "hal_spi_rpi.h"
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio/consumer.h>

#define SPI_CS_OFFSET   0x00
#define SPI_FIFO_OFFSET 0x04
#define SPI_CLK_OFFSET  0x08
#define SPI_DLEN_OFFSET 0x0C

/* Static memory maps for peripheral access */
static void __iomem *spi_base_addr = NULL;
static struct gpio_desc *dc_gpio = NULL;
static struct gpio_desc *reset_gpio = NULL;

static void RPi_WriteCommand(uint8_t cmd)
{
    if (!spi_base_addr || !dc_gpio) return;

    /* Assert Data/Command pin LOW for command transmission */
    gpiod_set_value(dc_gpio, 0);

    /* Transmit via SPI FIFO */
    writel((uint32_t)cmd, spi_base_addr + SPI_FIFO_OFFSET);

    /* Poll Transmission Done (DONE) bit in Control and Status register */
    while (!(readl(spi_base_addr + SPI_CS_OFFSET) & (1 << 16))) {
        cpu_relax();
    }
}

static void RPi_WriteData(uint8_t data)
{
    if (!spi_base_addr || !dc_gpio) return;

    /* Assert Data/Command pin HIGH for data transmission */
    gpiod_set_value(dc_gpio, 1);

    writel((uint32_t)data, spi_base_addr + SPI_FIFO_OFFSET);

    while (!(readl(spi_base_addr + SPI_CS_OFFSET) & (1 << 16))) {
        cpu_relax();
    }
}

static void RPi_HardReset(void)
{
    if (!reset_gpio) return;

    gpiod_set_value(reset_gpio, 1);
    msleep(50);
    gpiod_set_value(reset_gpio, 0);
    msleep(150);
    gpiod_set_value(reset_gpio, 1);
    msleep(50);
}

static void RPi_DelayMS(uint32_t ms)
{
    msleep(ms);
}

static const spi_driver_ops_t rpi_spi_ops = {
    .write_command = RPi_WriteCommand,
    .write_data    = RPi_WriteData,
    .hard_reset    = RPi_HardReset,
    .delay_ms      = RPi_DelayMS
};

const spi_driver_ops_t* RPi_SPI_Init(struct platform_device *pdev)
{
    /* BCM2711 SPI0 Physical Base Address mapping */
    /* In a strict production environment, this is extracted via platform_get_resource */
    /* Fallback mapping implemented for explicit MMIO requirement */
    spi_base_addr = ioremap(0xFE204000, 0x100); 
    if (!spi_base_addr) {
        pr_err("hal_spi_rpi: Failed to map SPI hardware registers.\n");
        return NULL;
    }

    /* Acquire GPIO descriptors defined in the Device Tree */
    dc_gpio = gpiod_get(&pdev->dev, "dc", GPIOD_OUT_LOW);
    if (IS_ERR(dc_gpio)) {
        pr_err("hal_spi_rpi: Failed to acquire Data/Command GPIO.\n");
        iounmap(spi_base_addr);
        return NULL;
    }

    reset_gpio = gpiod_get(&pdev->dev, "reset", GPIOD_OUT_HIGH);
    if (IS_ERR(reset_gpio)) {
        pr_err("hal_spi_rpi: Failed to acquire Reset GPIO.\n");
        gpiod_put(dc_gpio);
        iounmap(spi_base_addr);
        return NULL;
    }

    /* Configure SPI Master: CLEAR TX/RX FIFOs, TA=1, CPOL=0, CPHA=0 */
    writel(0x000000B0, spi_base_addr + SPI_CS_OFFSET);

    return &rpi_spi_ops;
}

void RPi_SPI_Deinit(void)
{
    if (dc_gpio) gpiod_put(dc_gpio);
    if (reset_gpio) gpiod_put(reset_gpio);
    if (spi_base_addr) iounmap(spi_base_addr);
}
