import ctypes
import os
import pytest
from typing import List, Tuple

ST7789_SWRESET = 0x01
ST7789_SLPOUT  = 0x11
ST7789_COLMOD  = 0x3A
ST7789_MADCTL  = 0x36
ST7789_INVON   = 0x21
ST7789_NORON   = 0x13
ST7789_DISPON  = 0x29

class SPIHardwareValidator:
    def __init__(self, lib_path: str):
        self.abs_path = os.path.abspath(lib_path)
        
        if not os.path.exists(self.abs_path):
            raise FileNotFoundError(f"Critical Error: Shared object not found at {self.abs_path}")
        
        self.lib = ctypes.CDLL(self.abs_path)
        
        self.lib.st7789_init_sequence.argtypes = [ctypes.c_void_p]
        self.lib.st7789_init_sequence.restype = None
        
        self.lib.mock_reset_trace.argtypes = []
        self.lib.mock_reset_trace.restype = None
        
        self.lib.mock_get_trace_size.argtypes = []
        self.lib.mock_get_trace_size.restype = ctypes.c_uint32
        
        self.lib.mock_is_command.argtypes = [ctypes.c_uint32]
        self.lib.mock_is_command.restype = ctypes.c_uint8
        
        self.lib.mock_get_value.argtypes = [ctypes.c_uint32]
        self.lib.mock_get_value.restype = ctypes.c_uint8
        
        self.mock_spi_ops_ptr = ctypes.addressof(ctypes.c_void_p.in_dll(self.lib, "mock_spi_ops"))

    def execute_init_sequence(self) -> None:
        self.lib.mock_reset_trace()
        self.lib.st7789_init_sequence(self.mock_spi_ops_ptr)

    def retrieve_trace(self) -> List[Tuple[bool, int]]:
        size = self.lib.mock_get_trace_size()
        trace = []
        for i in range(size):
            is_cmd = bool(self.lib.mock_is_command(i))
            val = self.lib.mock_get_value(i)
            trace.append((is_cmd, val))
        return trace

@pytest.fixture(scope="module")
def hardware_validator() -> SPIHardwareValidator:
    base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    lib_name = "libengine_mock_shared.so"
    
    search_paths = [
        os.path.join(base_dir, "build_host", lib_name),
        os.path.join(base_dir, "build", lib_name),
        os.path.join(base_dir, lib_name)
    ]
    
    for path in search_paths:
        if os.path.exists(path):
            return SPIHardwareValidator(path)
            
    pytest.fail(f"Compilation prerequisite missing. Searched: {search_paths}")

def test_st7789_initialization_sequence(hardware_validator: SPIHardwareValidator):
    hardware_validator.execute_init_sequence()
    trace = hardware_validator.retrieve_trace()
    
    assert len(trace) > 0, "No SPI operations were registered during initialization."
    
    assert trace[0] == (True, ST7789_SWRESET), "Boot sequence failure: Must initiate with SWRESET (0x01)"
    
    assert trace[1] == (True, ST7789_SLPOUT), "Power state failure: Must transmit SLPOUT (0x11)"
    
    colmod_idx = trace.index((True, ST7789_COLMOD))
    assert trace[colmod_idx + 1] == (False, 0x55), "Color mode violation: RGB565 requires data 0x55"
    
    assert (True, ST7789_NORON) in trace, "Display operation failure: Missing NORON (0x13)"
    assert trace[-1] == (True, ST7789_DISPON), "Termination sequence failure: Final command must be DISPON (0x29)"