#ifndef BCM2837_HW_H
#define BCM2837_HW_H

#include <stdint.h>

#define BCM2837_PERI_BASE       0x3F000000U
#define BCM2837_GPIO_BASE       (BCM2837_PERI_BASE + 0x200000U)
#define BCM2837_SPI0_BASE       (BCM2837_PERI_BASE + 0x204000U)

/* SPI0 Register Bitmasks */
#define SPI_CS_TXD              (1U << 18)
#define SPI_CS_RXD              (1U << 17)
#define SPI_CS_DONE             (1U << 16)
#define SPI_CS_TA               (1U << 7)
#define SPI_CS_CLEAR_RX         (1U << 5)
#define SPI_CS_CLEAR_TX         (1U << 4)

/* GPIO Function Select */
#define GPIO_INPUT              0U
#define GPIO_OUTPUT             1U
#define GPIO_ALT0               4U

/* Memory Barrier: Enforces CPU execution order for MMIO */
static inline void hw_memory_barrier(void) {
    __asm__ volatile("dmb sy" ::: "memory");
}

#endif /* BCM2837_HW_H */