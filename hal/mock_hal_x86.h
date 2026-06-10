#ifndef MOCK_HAL_X86_H
#define MOCK_HAL_X86_H

#include <stdint.h>

/* FFI exposed functions for Python ctypes integration */
void Mock_ResetTrace(void);
uint32_t Mock_GetTraceSize(void);
uint8_t Mock_IsCommand(uint32_t index);
uint8_t Mock_GetValue(uint32_t index);

#endif /* MOCK_HAL_X86_H */
