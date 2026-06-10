# ------------------------------------------------------------------------------
# Kernel Module Build Specification
# ------------------------------------------------------------------------------
obj-m += st7789_fb.o

# Specify cross-compilation environment parameters
ARCH ?= arm64
CROSS_COMPILE ?= aarch64-linux-gnu-

# Fallback to local kernel headers
KDIR ?= /lib/modules/$(shell uname -r)/build

all:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

