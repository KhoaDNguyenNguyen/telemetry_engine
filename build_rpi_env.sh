#!/usr/bin/env bash
# Enforce strict error handling
set -euo pipefail

KERNEL_BRANCH="rpi-6.1.y"
JOBS=$(nproc)
CROSS_PREFIX="aarch64-linux-gnu-"

echo "[1/4] Preparing Linux Kernel and injecting ST7789 driver..."
if [ ! -d "linux" ]; then
    git clone --depth=1 -b ${KERNEL_BRANCH} https://github.com/raspberrypi/linux.git
fi

# Inject the custom boot logo
cp logo_linux_clut224.ppm linux/drivers/video/logo/logo_linux_clut224.ppm

# Flatten driver structure for Kernel inclusion
DRV_DIR="linux/drivers/video/fbdev/telemetry"
mkdir -p ${DRV_DIR}

cp driver/st7789_fb.c ${DRV_DIR}/
cp driver/hal_spi_rpi.c ${DRV_DIR}/
cp driver/hal_spi_rpi.h ${DRV_DIR}/
cp core/src/st7789_logic.c ${DRV_DIR}/
cp core/inc/st7789_logic.h ${DRV_DIR}/
cp hal/hal_spi.h ${DRV_DIR}/

# Rewrite include paths to match the flattened kernel directory structure
sed -i 's|../core/inc/st7789_logic.h|st7789_logic.h|g' ${DRV_DIR}/st7789_fb.c
sed -i 's|../hal/hal_spi.h|hal_spi.h|g' ${DRV_DIR}/hal_spi_rpi.h

# Generate Kernel Makefile for the telemetry driver
cat << 'EOF' > ${DRV_DIR}/Makefile
obj-y += telemetry_fb.o
telemetry_fb-objs := st7789_fb.o hal_spi_rpi.o st7789_logic.o
ccflags-y := -Wall -Werror
EOF

# Link the telemetry directory to the parent fbdev Makefile
if ! grep -q "telemetry/" linux/drivers/video/fbdev/Makefile; then
    echo "obj-y += telemetry/" >> linux/drivers/video/fbdev/Makefile
fi

cd linux
if [ ! -f ".config" ]; then
    make ARCH=arm64 CROSS_COMPILE=${CROSS_PREFIX} bcmrpi3_defconfig
fi
make ARCH=arm64 CROSS_COMPILE=${CROSS_PREFIX} Image dtbs -j${JOBS}
cd ..

echo "[2/4] Compiling statically linked Kiosk App..."
rm -rf build && mkdir build && cd build
cmake -DCMAKE_C_COMPILER=${CROSS_PREFIX}gcc -DCMAKE_C_FLAGS="-static -O3" ..
make kiosk_telemetry_app -j${JOBS}
cd ..

echo "[3/4] Building Busybox, Dropbear, Stress and Rootfs..."
mkdir -p rpi_env && cd rpi_env

# 3.1 Busybox
if [ ! -d "busybox-1.36.1" ]; then
    wget -qO- https://busybox.net/downloads/busybox-1.36.1.tar.bz2 | tar -xj
fi
cd busybox-1.36.1
make ARCH=arm64 CROSS_COMPILE=${CROSS_PREFIX} defconfig
sed -i 's/# CONFIG_STATIC is not set/CONFIG_STATIC=y/' .config
sed -i 's/CONFIG_TC=y/# CONFIG_TC is not set/' .config
make ARCH=arm64 CROSS_COMPILE=${CROSS_PREFIX} install CONFIG_PREFIX=../rootfs -j${JOBS}
cd ..

# 3.2 Dropbear (SSH Server)
if [ ! -d "dropbear-2022.83" ]; then
    wget -qO- https://matt.ucc.asn.au/dropbear/releases/dropbear-2022.83.tar.bz2 | tar -xj
fi
cd dropbear-2022.83
if [ ! -f "dropbear" ]; then
    ./configure --host=aarch64-linux-gnu --disable-zlib --enable-static
    
    # Disable password authentication to bypass crypt() dependency
    echo "#define DROPBEAR_SVR_PASSWORD_AUTH 0" > localoptions.h
    
    make -j${JOBS}
fi
mkdir -p ../rootfs/usr/sbin ../rootfs/etc/dropbear
cp dropbear dropbearkey ../rootfs/usr/sbin/
cd ..

# 3.3 Stress
if [ ! -d "stress-1.0.4" ]; then
    wget -qO- https://src.fedoraproject.org/repo/pkgs/stress/stress-1.0.4.tar.gz/a607afa695a511765b40993a64c6e2f4/stress-1.0.4.tar.gz | tar -xz
fi
cd stress-1.0.4
if [ ! -f "src/stress" ]; then
    ./configure --host=aarch64-linux-gnu LDFLAGS="-static"
    make -j${JOBS}
fi
mkdir -p ../rootfs/usr/bin
cp src/stress ../rootfs/usr/bin/
cd ..

echo "[4/4] Assembling RootFS..."
cd rootfs
mkdir -p dev proc sys tmp root etc root/.ssh

# Mount host SSH public keys for passwordless authentication
if ls ~/.ssh/*.pub 1> /dev/null 2>&1; then
    cat ~/.ssh/*.pub > root/.ssh/authorized_keys
    chmod 700 root/.ssh
    chmod 600 root/.ssh/authorized_keys
else
    echo "WARNING: No SSH keys found in ~/.ssh/. Generate one using 'ssh-keygen -t ed25519' before building if you need SSH access."
fi

echo "root:x:0:0:root:/root:/bin/sh" > etc/passwd
echo "root::10933:0:99999:7:::" > etc/shadow

cp ../../build/kiosk_telemetry_app root/

cat << 'EOF' > init
#!/bin/sh

mount -t proc none /proc
mount -t sysfs none /sys
mount -t devtmpfs none /dev

echo "Bringing up eth0 and requesting DHCP lease..."
ip link set eth0 up
udhcpc -i eth0 -t 5 -q &

echo "Initializing Dropbear SSH Server..."
if [ ! -f /etc/dropbear/dropbear_rsa_host_key ]; then
    /usr/sbin/dropbearkey -t rsa -f /etc/dropbear/dropbear_rsa_host_key
fi

# Execute Dropbear with key-based authentication only (-R generates host keys)
/usr/sbin/dropbear -R

echo "Executing 3D Telemetry Kiosk..."
/root/kiosk_telemetry_app &

exec setsid cttyhack sh < /dev/console > /dev/console 2>&1
EOF
chmod +x init

# Package initramfs into a cpio archive
find . | cpio -H newc -o > ../rootfs.cpio
cd ../..

echo "Build complete. Artifacts are located in rpi_env/rootfs.cpio and linux/arch/arm64/boot/Image"