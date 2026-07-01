#define _DEFAULT_SOURCE

#include "hal_rpi.h"
#include "bcm2837_hw.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <stddef.h>
#include <unistd.h>

typedef struct {
    volatile uint32_t cs;
    volatile uint32_t fifo;
    volatile uint32_t clk;
    volatile uint32_t dlen;
    volatile uint32_t ltoh;
    volatile uint32_t dc;
} spi_hw_regs_t;

static volatile uint32_t *gpio_regs = NULL;
static spi_hw_regs_t *spi_regs = NULL;

#define PIN_DC 24
#define PIN_RES 25

int hal_rpi_init(int mem_fd) {
    gpio_regs = (volatile uint32_t *)mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, (off_t)BCM2837_GPIO_BASE);
    spi_regs = (spi_hw_regs_t *)mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, (off_t)BCM2837_SPI0_BASE);
    
    if (gpio_regs == MAP_FAILED || spi_regs == MAP_FAILED) {
        return -1;
    }

    spi_regs->clk = 4U; 
    spi_regs->cs = 0U;
    return 0;
}

void hal_gpio_set_function(uint8_t pin, uint8_t mode) {
    if (!gpio_regs) return;
    uint8_t reg_index = pin / 10;
    uint8_t bit_shift = (pin % 10) * 3;
    
    uint32_t reg_val = gpio_regs[reg_index];
    reg_val &= ~((uint32_t)7 << bit_shift);
    reg_val |= ((uint32_t)mode << bit_shift);
    
    hw_memory_barrier();
    gpio_regs[reg_index] = reg_val;
    hw_memory_barrier();
}

static inline void gpio_set(uint8_t pin) {
    gpio_regs[7] = ((uint32_t)1 << pin);
}

static inline void gpio_clear(uint8_t pin) {
    gpio_regs[10] = ((uint32_t)1 << pin);
}

static void rpi_write_command(uint8_t cmd) {
    hw_memory_barrier();
    gpio_clear(PIN_DC);
    hw_memory_barrier();
    
    spi_regs->cs = SPI_CS_TA | SPI_CS_CLEAR_TX | SPI_CS_CLEAR_RX; 
    while (!(spi_regs->cs & SPI_CS_TXD)) {} 
    spi_regs->fifo = (uint32_t)cmd; 
    while (!(spi_regs->cs & SPI_CS_DONE)) {} 
    spi_regs->cs = 0U;
    
    hw_memory_barrier();
}

static void rpi_write_data(uint8_t data) {
    hw_memory_barrier();
    gpio_set(PIN_DC);
    hw_memory_barrier();
    
    spi_regs->cs = SPI_CS_TA; 
    while (!(spi_regs->cs & SPI_CS_TXD)) {}
    spi_regs->fifo = (uint32_t)data;
    while (!(spi_regs->cs & SPI_CS_DONE)) {}
    spi_regs->cs = 0U;
    
    hw_memory_barrier();
}

static void rpi_write_buffer(const uint16_t *buffer, uint32_t pixel_count) {
    hw_memory_barrier();
    gpio_set(PIN_DC);
    hw_memory_barrier();
    
    spi_regs->cs = SPI_CS_TA | SPI_CS_CLEAR_TX | SPI_CS_CLEAR_RX; 
    
    for (uint32_t i = 0U; i < pixel_count; i++) {
        const uint16_t pixel = buffer[i];
        
        while (!(spi_regs->cs & SPI_CS_TXD)) {
            if (spi_regs->cs & SPI_CS_RXD) { (void)spi_regs->fifo; }
        }
        spi_regs->fifo = (uint32_t)(pixel >> 8);
        
        while (!(spi_regs->cs & SPI_CS_TXD)) {
            if (spi_regs->cs & SPI_CS_RXD) { (void)spi_regs->fifo; }
        }
        spi_regs->fifo = (uint32_t)(pixel & 0xFFU);
        
        if (spi_regs->cs & SPI_CS_RXD) { (void)spi_regs->fifo; }
    }
    
    while (!(spi_regs->cs & SPI_CS_DONE)) {
        if (spi_regs->cs & SPI_CS_RXD) { (void)spi_regs->fifo; }
    }
    spi_regs->cs = 0U;
    hw_memory_barrier();
}

static void rpi_hard_reset(void) {
    hw_memory_barrier();
    gpio_set(PIN_RES); usleep(100000);
    gpio_clear(PIN_RES); usleep(100000);
    gpio_set(PIN_RES); usleep(100000);
    hw_memory_barrier();
}

static void rpi_delay_ms(uint32_t ms) {
    usleep(ms * 1000);
}

static const spi_driver_ops_t rpi_spi_ops = {
    .write_command = rpi_write_command,
    .write_data    = rpi_write_data,
    .write_buffer  = rpi_write_buffer,
    .hard_reset    = rpi_hard_reset,
    .delay_ms      = rpi_delay_ms
};

const spi_driver_ops_t* hal_rpi_get_spi_ops(void) {
    return &rpi_spi_ops;
}