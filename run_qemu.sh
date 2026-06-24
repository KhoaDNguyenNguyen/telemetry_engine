#!/usr/bin/env bash
qemu-system-aarch64 \
    -M raspi3b -m 1G \
    -kernel linux/arch/arm64/boot/Image \
    -dtb linux/arch/arm64/boot/dts/broadcom/bcm2837-rpi-3-b.dtb \
    -initrd rpi_env/rootfs.cpio \
    -append "console=ttyAMA0 root=/dev/ram0 rw init=/init loglevel=7" \
    -nographic \
    -netdev user,id=net0,hostfwd=tcp::2222-:22 \
    -device usb-net,netdev=net0
