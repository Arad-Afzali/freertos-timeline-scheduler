# Timeline Scheduler — FreeRTOS Implementation

**Group 50 — Embedded Operating Systems (2025/26)**  
**Politecnico di Torino**

---

## Team Members & Roles

| Member | Role | Key Contributions |
|--------|------|-------------------|
| **Arad** | Project Lead & Configuration | Public API design, `scheduler_config.h`, `timeline_config.c` |
| **Ali** | Kernel Developer | Cyclic executive engine (`cyclic.c/h`), FreeRTOS kernel integration |
| **Thiago** | Lifecycle Management | Task lifecycle (`lifecycle_manager.c/h`), frame reset, SRT scheduling |
| **Alessandro** | Trace & Debug | Trace ring-buffer system (`trace_optimized.c`, `trace.h`) |
| **Stefano** | QA & Environment Lead | Test harness (`main.c`), test automation, CI environment |

---

## Project Overview

This project extends the FreeRTOS kernel with a **timeline-driven (cyclic executive) scheduler** targeting the ARM Cortex-M3 architecture, emulated on QEMU.

The scheduler enforces a periodic **major frame** of configurable length (default 1000 ms). Within each frame:

- **Hard Real-Time (HRT)** tasks are released at fixed offsets by a tick-level cyclic executive hook and must complete before their deadlines. Violations are detected and the offending task is terminated.
- **Soft Real-Time (SRT)** tasks are dispatched during idle gaps when no HRT task is active. They run at a lower priority and are preempted transparently whenever an HRT window begins.
- At the end of each major frame, all tasks are destroyed and recreated from the original configuration to guarantee deterministic initial conditions.

A **trace system** records every task start, completion, deadline miss, and frame boundary event into a ring buffer. At the end of a test run the full trace log and summary statistics are printed over UART.

**Target platform:** LM3S6965 (ARM Cortex-M3) emulated with QEMU `lm3s6965evb`.

---

## Repository Structure

```
group50/
├── include/
│   ├── FreeRTOSConfig.h          # FreeRTOS kernel configuration
│   └── scheduler_config.h        # Timeline data types and API
├── src/
│   ├── main.c                    # System init, test tasks, FreeRTOS hooks
│   ├── timeline_config.c/h       # Configuration validation
│   ├── lifecycle_manager.c/h     # Task create/delete, SRT dispatch, frame reset
│   ├── trace_optimized.c         # Trace ring-buffer implementation
│   ├── trace.h                   # Trace public API
│   ├── startup.c                 # Cortex-M3 vector table and Reset_Handler
│   └── uart_io.c                 # UART0 driver and newlib syscall stubs
├── FreeRTOS-Kernel/              # FreeRTOS source (checked out from repo)
│   ├── cyclic.c/h                # Cyclic executive tick hook (Ali)
│   └── ...                       # Standard FreeRTOS kernel files
├── tests/
│   └── test_scheduler.py         # Static analysis tests (5 tests)
├── tools/
│   └── config_generator.py       # JSON → C config converter
├── docs/
│   └── CODE_QUALITY_REPORT.md
├── Makefile
├── linker_script.ld
└── README.md
```

---

## Prerequisites

| Tool | Tested Version | Notes |
|------|----------------|-------|
| `arm-none-eabi-gcc` | 15.2.Rel1 | ARM GNU Toolchain for bare-metal Cortex-M |
| `qemu-system-arm` | 9.2+ | Standard QEMU with `lm3s6965evb` machine support |
| GNU Make | 3.81+ | Build system |
| Python 3 | 3.10+ | Required only for the test suites |

### macOS (Homebrew)

```bash
brew install --cask gcc-arm-embedded   # ARM GCC toolchain
brew install qemu                       # QEMU system emulator
```

### Ubuntu / Debian

```bash
sudo apt-get update
sudo apt-get install gcc-arm-none-eabi qemu-system-arm make python3
```

### Windows (via WSL)

The Makefile relies on Unix commands (`mkdir -p`, `rm -rf`), so native CMD or PowerShell will not work. The recommended approach is to use **WSL** (Windows Subsystem for Linux):

1. Open PowerShell as Administrator and install WSL:
   ```powershell
   wsl --install
   ```
2. Restart your machine, then open the **Ubuntu** terminal that WSL installs.
3. Inside WSL, follow the Ubuntu instructions above:
   ```bash
   sudo apt-get update
   sudo apt-get install gcc-arm-none-eabi qemu-system-arm make python3
   ```
4. Clone or navigate to the project directory (your Windows files are accessible under `/mnt/c/...`).

> **Note:** If you prefer not to use WSL, you can also use **MSYS2** or **Git Bash**, which provide the same Unix utilities. In that case, install the ARM GCC toolchain and QEMU manually and add them to your `PATH`.

After installation, verify the toolchain:

```bash
arm-none-eabi-gcc --version
qemu-system-arm --version
```

---

## Getting the FreeRTOS Kernel Source

The FreeRTOS kernel is stored on the `integration-all` branch of this repository. After cloning, check it out:

```bash
git clone https://baltig.polito.it/eos25/group50.git
cd group50
git checkout origin/integration-all -- FreeRTOS-Kernel
```

This places the full `FreeRTOS-Kernel/` directory (including the custom `cyclic.c/h`) into the working tree.

---

## Building

```bash
make clean
make all
```

A successful build produces `build/scheduler.elf` and prints memory usage:

```
===== Build Complete =====
   text    data     bss     dec     hex filename
  26956     228   42340   69524   10f14 build/scheduler.elf
```

---

## Running on QEMU

```bash
qemu-system-arm \
    -machine lm3s6965evb \
    -kernel build/scheduler.elf \
    -serial file:/tmp/qemu_serial.log \
    -monitor none \
    -nographic &

# Let it run for a few seconds, then stop QEMU:
sleep 8
pkill qemu-system-arm

# Inspect the output:
cat /tmp/qemu_serial.log
```

Alternatively, to see output directly in the terminal (stop with `Ctrl-A X`):

```bash
qemu-system-arm \
    -machine lm3s6965evb \
    -kernel build/scheduler.elf \
    -serial stdio \
    -monitor none \
    -nographic
```

### Expected Output

The scheduler boots, creates four tasks, and begins cycling through major frames. Every frame the trace system logs HRT activations at their configured offsets and SRT activity during idle gaps. After the configured number of frames, a full trace dump and statistics summary are printed:

```
[HRT_A] Activated at tick 100
[HRT_A] Completed at tick 100
[HRT_B] Activated at tick 300
[HRT_B] Completed at tick 301
[FRAME_RESET] Frame 1 completed at tick 1000, resetting...
...
========== TRACE STATISTICS ==========
Deadline Misses:     0
Period Overruns:     0
Tasks Completed:     N
Tasks Terminated:    0
======================================
```

---

## Test Configuration

The default test harness in `main.c` defines the following timeline:

| Task | Type | Start (tick) | Deadline (tick) | Subframe |
|------|------|-------------|-----------------|----------|
| HRT_A | Hard RT | 100 | 200 | 1 |
| HRT_B | Hard RT | 300 | 450 | 2 |
| SRT_X | Soft RT | — | — | — |
| SRT_Y | Soft RT | — | — | — |

Major frame period: **1000 ticks** (= 1000 ms at `configTICK_RATE_HZ = 1000`).

---

## Running the Tests

```bash
python3 tests/test_scheduler.py
```

The test suite verifies configuration correctness at the source level:
- HRT time-window overlap detection
- Timing parameter validation
- FreeRTOS configuration consistency
- Build success
- Memory usage within hardware limits

---

## Architecture

### Cyclic Executive (`cyclic.c`)

A tick-level ISR hook (`xCyclicExecTickHookFromISR`) is called from `xTaskIncrementTick()` in the FreeRTOS kernel. It maintains a sorted table of HRT entries and sends a task notification to each HRT task exactly when the relative tick within the current major frame matches the task's configured start time. This provides deterministic, non-polling activation with single-tick resolution.

### Lifecycle Manager (`lifecycle_manager.c`)

A high-priority manager task runs every tick and performs two duties:
1. **Deadline enforcement** — checks whether any running HRT task has exceeded its end-time and terminates it if so.
2. **SRT dispatch** — when no HRT task is active, resumes the next suspended SRT task in round-robin order.

At each major-frame boundary, a one-shot software timer triggers a full reset: all tasks are deleted, runtime state is cleared, tasks are recreated from the original configuration, and the cyclic executive table is rebuilt with the new task handles.

### Trace System (`trace_optimized.c`)

A fixed-size ring buffer (512 entries) records timestamped events. All insertions are protected by critical sections so the trace can be called from both task and ISR context. At the end of a test run, `vTracePrintAllEvents()` iterates the buffer and prints each event over UART.

---

## Key Design Decisions

| Decision | Rationale |
|----------|-----------|
| Non-preemptive HRT, preemptive SRT | HRT tasks run to completion within their window; SRT tasks yield to HRT via priority difference |
| Destroy-and-recreate at frame reset | Guarantees clean initial state every frame with no residual stack or notification artifacts |
| Notification-based HRT activation | `ulTaskNotifyTake` / `vTaskNotifyGiveFromISR` is lighter than semaphores and avoids shared-resource contention |
| UART polled I/O for printf | Simplest approach for QEMU; no DMA or interrupt-driven TX needed |
| 3 NVIC priority bits | Matches the LM3S6965 hardware (8 priority levels) |
