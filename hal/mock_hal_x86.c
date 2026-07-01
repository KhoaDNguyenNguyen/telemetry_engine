#include "hal_spi.h"
#include "mock_hal_x86.h"

#define MOCK_TRACE_SIZE 1024

typedef struct {
    uint8_t is_command;
    uint8_t value;
} spi_trace_t;

static spi_trace_t trace_buffer[MOCK_TRACE_SIZE];
static uint32_t trace_idx = 0;

static void mock_write_command(uint8_t cmd) {
    if (trace_idx < MOCK_TRACE_SIZE) {
        trace_buffer[trace_idx].is_command = 1;
        trace_buffer[trace_idx].value = cmd;
        trace_idx++;
    }
}

static void mock_write_data(uint8_t data) {
    if (trace_idx < MOCK_TRACE_SIZE) {
        trace_buffer[trace_idx].is_command = 0;
        trace_buffer[trace_idx].value = data;
        trace_idx++;
    }
}

static void mock_write_buffer(const uint16_t *buffer, uint32_t pixel_count) {
    (void)buffer;
    (void)pixel_count;
}

static void mock_hard_reset(void) {}
static void mock_delay_ms(uint32_t ms) { (void)ms; }

const spi_driver_ops_t mock_spi_ops = {
    .write_command = mock_write_command,
    .write_data    = mock_write_data,
    .write_buffer  = mock_write_buffer,
    .hard_reset    = mock_hard_reset,
    .delay_ms      = mock_delay_ms
};

void mock_reset_trace(void) { trace_idx = 0; }
uint32_t mock_get_trace_size(void) { return trace_idx; }
uint8_t mock_is_command(uint32_t i) { return i < trace_idx ? trace_buffer[i].is_command : 0; }
uint8_t mock_get_value(uint32_t i) { return i < trace_idx ? trace_buffer[i].value : 0; }