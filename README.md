# Timeline Scheduler - FreeRTOS Implementation

**Group 50 - Real-Time Operating Systems**  
**University Course Project - January 2026**

---

## Team Members & Roles

| Member | Role | Responsibilities |
|--------|------|------------------|
| **Arad** | Project Lead & Configuration | Public API design, scheduler configuration |
| **Ali** | Kernel Developer | Timeline scheduler kernel, lifecycle management |
| **Alessandro** | Trace & Debug | Trace system, logging, visualization |
| **Thiago** | Task Implementation | Application task development |
| **Stefano** | QA & Environment Lead | Test automation, environment setup, quality assurance |

---

## Project Overview

This project implements a **Timeline Scheduler** for FreeRTOS, supporting:
- ✅ **Hard Real-Time (HRT) Tasks** - Guaranteed execution windows with deadlines
- ✅ **Soft Real-Time (SRT) Tasks** - Best-effort execution during idle gaps
- ✅ **Major Frame Scheduling** - Cyclic executive with 1000ms periods
- ✅ **Deadline Enforcement** - Automatic termination of deadline violators
- ✅ **Trace System** - Comprehensive event logging for analysis

**Target Platform:** ARM Cortex-M3 (QEMU lm3s6965evb)

---

## Development Environment Setup

### Prerequisites

To ensure **"it works on my machine"** consistency, all team members must use the exact same toolchain versions.

#### Required Tools & Versions

| Tool | Version | Download Link |
|------|---------|---------------|
| **ARM GCC Toolchain** | 13.2.Rel1 | [ARM Developer](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads) |
| **xPack QEMU Arm** | 8.2.6-1 | [xPack Releases](https://github.com/xpack-dev-tools/qemu-arm-xpack/releases/) |
| **GNU Make** | 4.4.1 | [Make for Windows](https://gnuwin32.sourceforge.net/packages/make.htm) |
| **Python** | 3.10+ | [Python.org](https://www.python.org/downloads/) |
| **VS Code** | Latest | [Visual Studio Code](https://code.visualstudio.com/) |

#### VS Code Extensions (Required)

Install these extensions in VS Code:
1. **C/C++** (Microsoft) - `ms-vscode.cpptools`
2. **Cortex-Debug** (marus25) - `marus25.cortex-debug`

---

### Installation Steps (Windows)

#### 1. Install ARM GCC Toolchain 13.2.Rel1

```powershell
# Download from ARM website
# Install to: C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\13.2 Rel1

# Add to PATH:
# C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\13.2 Rel1\bin

# Verify installation:
arm-none-eabi-gcc --version
# Expected output: arm-none-eabi-gcc (Arm GNU Toolchain 13.2.rel1) 13.2.1
