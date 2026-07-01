#!/usr/bin/env bash
set -euo pipefail

WORKSPACE_DIR="$(pwd)"
BUILD_DIR="${WORKSPACE_DIR}/build"
CACHE_DIR="${WORKSPACE_DIR}/.fw_cache"
ROOTFS_DIR="${WORKSPACE_DIR}/rootfs_tmp"
OUTPUT_CPIO="${WORKSPACE_DIR}/rootfs.cpio"

if ! command -v aarch64-linux-gnu-gcc &> /dev/null; then
    echo "[ERROR] Toolchain aarch64-linux-gnu-gcc not found in PATH."
    exit 1
fi

mkdir -p "${CACHE_DIR}"
FW_FILES=("bootcode.bin" "start.elf" "fixup.dat" "kernel8.img" "bcm2710-rpi-3-b.dtb")

for fw in "${FW_FILES[@]}"; do
    if [[ ! -s "${CACHE_DIR}/${fw}" ]]; then
        curl -sL --fail -o "${CACHE_DIR}/${fw}" "https://github.com/raspberrypi/firmware/raw/master/boot/${fw}"
    fi
done

rm -rf "${BUILD_DIR}" && mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# Compile strictly static executable for isolated Initramfs
cmake -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc -DCMAKE_C_FLAGS="-static -O3 -Wall -Werror" ..

make kiosk_telemetry_app -j"$(nproc)"

cd "${WORKSPACE_DIR}"

rm -rf "${ROOTFS_DIR}" && mkdir -p "${ROOTFS_DIR}"
mkdir -p "${ROOTFS_DIR}/"{dev,sys,proc}

cp "${BUILD_DIR}/kiosk_telemetry_app" "${ROOTFS_DIR}/init"
chmod +x "${ROOTFS_DIR}/init"

cd "${ROOTFS_DIR}"
find . | cpio -H newc -o > "${OUTPUT_CPIO}" 2>/dev/null
cd "${WORKSPACE_DIR}"
rm -rf "${ROOTFS_DIR}"

echo "[SUCCESS] Initramfs assembled at: ${OUTPUT_CPIO}"