#!/usr/bin/env bash
set -euo pipefail

WORKSPACE_DIR="$(pwd)"
CACHE_DIR="${WORKSPACE_DIR}/.fw_cache"
ROOTFS_CPIO="${WORKSPACE_DIR}/rootfs.cpio"

if [[ ! -f "${ROOTFS_CPIO}" ]]; then
    echo "[ERROR] Payload rootfs.cpio missing."
    exit 1
fi

TARGET_MOUNT=$(lsblk -o MOUNTPOINT,FSTYPE,RM -n -r | awk '$2=="vfat" && $3=="1" && $1!="" && $1!~"^/boot" {print $1}')
MOUNT_COUNT=$(echo "${TARGET_MOUNT}" | wc -w)

if [[ ${MOUNT_COUNT} -ne 1 || -z "${TARGET_MOUNT}" ]]; then
    echo "[ERROR] Target resolution failed. Ensure exactly 1 removable FAT32 SD Card is inserted."
    exit 1
fi

echo "[WARN] Target partition resolved to: ${TARGET_MOUNT}"
read -p "Proceed with destructive synchronization? (y/N) " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    exit 1
fi

sudo find "${TARGET_MOUNT}" -mindepth 1 -delete
sudo cp "${CACHE_DIR}/"* "${TARGET_MOUNT}/"
sudo cp "${ROOTFS_CPIO}" "${TARGET_MOUNT}/"

sudo bash -c "cat << 'EOF' > ${TARGET_MOUNT}/config.txt
arm_64bit=1
kernel=kernel8.img
device_tree=bcm2710-rpi-3-b.dtb
initramfs rootfs.cpio followkernel
hdmi_force_hotplug=1
force_turbo=1
EOF"

sudo sync
echo "[SUCCESS] SD Card deployment complete."