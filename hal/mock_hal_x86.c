#include "hal_spi.h"
#include "mock_hal_x86.h"
#include <stdint.h>

#define MOCK_TRACE_SIZE 1024

typedef struct {
    uint8_t is_command;
    uint8_t value;
} spi_trace_t;

static spi_trace_t mock_trace_buffer[MOCK_TRACE_SIZE];
static uint32_t mock_trace_index = 0;

static void Mock_WriteCommand(uint8_t cmd) {
    if (mock_trace_index < MOCK_TRACE_SIZE) {
        mock_trace_buffer[mock_trace_index].is_command = 1;
        mock_trace_buffer[mock_trace_index].value = cmd;
        mock_trace_index++;
    }
}

static void Mock_WriteData(uint8_t data) {
    if (mock_trace_index < MOCK_TRACE_SIZE) {
        mock_trace_buffer[mock_trace_index].is_command = 0;
        mock_trace_buffer[mock_trace_index].value = data;
        mock_trace_index++;
    }
}

static void Mock_HardReset(void) {
    /* No-operation in virtual environment */
}

static void Mock_DelayMS(uint32_t ms) {
    /* Explicitly suppress delays during automated testing */
    (void)ms;
}

const spi_driver_ops_t mock_spi_ops = {
    Mock_WriteCommand,
    Mock_WriteData,
    Mock_HardReset,
    Mock_DelayMS
};

/* --- Application Programming Interfaces for Python FFI Validation --- */

void Mock_ResetTrace(void) {
    mock_trace_index = 0;
}

uint32_t Mock_GetTraceSize(void) {
    return mock_trace_index;
}

uint8_t Mock_IsCommand(uint32_t index) {
    if (index >= mock_trace_index) return 0;
    return mock_trace_buffer[index].is_command;
}

uint8_t Mock_GetValue(uint32_t index) {
    if (index >= mock_trace_index) return 0;
    return mock_trace_buffer[index].value;
}
