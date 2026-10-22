#!/usr/bin/env bash
set -euo pipefail

WORKSPACE_DIR="$(pwd)"
FW_CACHE="${WORKSPACE_DIR}/.fw_cache"
KERNEL_IMAGE="${FW_CACHE}/kernel8.img"
DTB_FILE="${FW_CACHE}/bcm2710-rpi-3-b.dtb"
INITRAMFS="${WORKSPACE_DIR}/rootfs.cpio"

if [[ ! -f "${KERNEL_IMAGE}" || ! -f "${INITRAMFS}" ]]; then
    echo "[ERROR] Missing execution artifacts. Run scripts/build_rootfs.sh first."
    exit 1
fi

echo "[INFO] Executing AArch64 system panic validation..."
echo "[INFO] Note: Application utilizes raw MMIO. QEMU will not render SPI outputs."
echo "[INFO] To terminate the emulator, press: Ctrl+A, then X."
echo "------------------------------------------------------------------------"

qemu-system-aarch64 \
    -M raspi3b \
    -m 1G \
    -kernel "${KERNEL_IMAGE}" \
    -dtb "${DTB_FILE}" \
    -initrd "${INITRAMFS}" \
    -append "console=ttyAMA0 root=/dev/ram0 rw init=/init loglevel=4 panic=1 iomem=relaxed iomem=relaxed iomem=relaxed" \
    -nographic \
    -serial mon:stdio