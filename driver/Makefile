# ------------------------------------------------------------------------------
# Kernel Module Build Specification
# ------------------------------------------------------------------------------

obj-m += telemetry_fb.o

# Consolidate platform driver, hardware abstraction, and mathematical core
telemetry_fb-objs := st7789_fb.o hal_spi_rpi.o ../core/src/st7789_logic.o

# Strict compilation flags adhering to ISO C11 and Kernel conventions
ccflags-y := -I$(src)/../core/inc -I$(src)/../hal -I$(src) -O3 -mcpu=cortex-a53 -Wall -Werror

ARCH ?= arm64
CROSS_COMPILE ?= aarch64-linux-gnu-
KDIR ?= /lib/modules/$(shell uname -r)/build

all:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
