#!/usr/bin/env bash
set -euo pipefail

WORKSPACE_DIR="$(pwd)"
CACHE_DIR="${WORKSPACE_DIR}/.fw_cache"
ROOTFS_CPIO="${WORKSPACE_DIR}/rootfs.cpio"

TARGET_MOUNT=$(lsblk -o MOUNTPOINT,FSTYPE,RM -n -r | awk '$2=="vfat" && $3=="1" && $1!="" && $1!~"^/boot" {print $1}')

if [[ -z "${TARGET_MOUNT}" ]]; then
    exit 1
fi

sudo find "${TARGET_MOUNT}" -mindepth 1 -delete
sudo cp "${CACHE_DIR}/"* "${TARGET_MOUNT}/"
sudo cp "${ROOTFS_CPIO}" "${TARGET_MOUNT}/"

sudo bash -c "cat << 'IN_EOF' > ${TARGET_MOUNT}/config.txt
arm_64bit=1
kernel=kernel8.img
device_tree=bcm2710-rpi-3-b.dtb
initramfs rootfs.cpio followkernel
hdmi_force_hotplug=1
force_turbo=1
IN_EOF"

sudo bash -c "echo 'console=serial0,115200 console=tty1 root=/dev/ram0 rw init=/init iomem=relaxed panic=10' > ${TARGET_MOUNT}/cmdline.txt"

sudo sync
