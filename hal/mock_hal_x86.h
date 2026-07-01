#ifndef MOCK_HAL_X86_H
#define MOCK_HAL_X86_H

#include <stdint.h>

/* Introspection APIs for Python ctypes validation */
void mock_reset_trace(void);
uint32_t mock_get_trace_size(void);
uint8_t mock_is_command(uint32_t index);
uint8_t mock_get_value(uint32_t index);

#endif /* MOCK_HAL_X86_H */