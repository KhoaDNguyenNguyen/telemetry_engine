# Telemetry Engine

A hardware-agnostic 3D kinematics and telemetry visualization engine. Configured to execute as the initial process (PID 1) on the Raspberry Pi 3 (BCM2837), utilizing direct Memory-Mapped I/O (MMIO) to interface with the ST7789 display controller.

## System Architecture

The codebase implements a strict Dependency Inversion Principle, decoupling hardware interactions from the mathematical and rendering pipelines.

*   `app/`: Entry points for target execution (PID 1) and host simulation (SDL2).
*   `core/`: Hardware-agnostic mathematical operations (Tait-Bryan rotations, Z-Depth shading) and display controller sequences.
*   `hal/`: Hardware Abstraction Layer mapping abstract operations to physical BCM2837 MMIO registers or x86_64 RAM buffers.
*   `scripts/`: Deterministic build, package, and deployment automation.
*   `tests/`: Software-In-the-Loop (SIL) validation matrices via Python FFI.

## Build and Deployment Requirements

*   **Target Hardware**: Raspberry Pi 3 Model B (BCM2837)
*   **Cross-Compiler**: `aarch64-linux-gnu-gcc`
*   **Host Environment**: Linux (Arch/Debian) with `cmake`, `make`, `cpio`.
*   **Validation Tools**: `python3` with `pytest` and `numpy`.

## Compilation and Execution

### 1. Software-In-the-Loop (SIL) Validation
Executes the mathematical validation suite and hardware protocol verification against the virtual RAM buffer.

```bash
mkdir -p build_host && cd build_host
cmake ..
make engine_mock_shared -j$(nproc)
cd ..
pytest tests/ -v
```

### 2. Desktop Simulation (Host)
Compiles the application natively on x86_64, utilizing SDL2 as the virtual framebuffer.

```bash
cd build_host
make simulator_desktop -j$(nproc)
./simulator_desktop
```

### 3. Hardware Deployment
Cross-compiles the system as a statically linked binary, packages it into a volatile initramfs (`rootfs.cpio`), and flashes the artifacts to the target SD Card.

```bash
./scripts/build_rootfs.sh
./scripts/deploy_sd.sh
```

## Thermal Calibration
The physical display engine translates CPU thermal data into a visual spectrum (Cyan to Magenta). The nominal operational range is configured to `[40.0°C, 57.0°C]`. Interpolation parameters are located within the primary application loop in `app/main.c`.