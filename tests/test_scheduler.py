#!/usr/bin/env python3
"""
Timeline Scheduler - Test Suite
Author: Stefano (QA & Environment Lead)
Date: January 14, 2026

Part A — Static checks (no hardware needed):
  1. HRT overlap       – no two HRT tasks share the same time window
  2. Timing validity   – every HRT window is well-formed and inside the major frame
  3. FreeRTOS config   – key kernel settings match expected values
  4. Build             – the project compiles without errors
  5. Memory            – FLASH/RAM usage stays within LM3S6965 limits

Part B — Runtime checks (run scheduler on QEMU and verify output):
  6. HRT activation    – every HRT task is activated each frame
  7. HRT timing        – HRT tasks start near their configured offset
  8. No deadline miss  – trace statistics report 0 deadline misses
  9. Frame reset       – frames are reset and tasks are recreated
 10. SRT execution     – SRT tasks run during idle gaps
"""

import sys, re, subprocess, os, time, signal

# ---------- paths ----------------------------------------------------------
SCRIPT_DIR  = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.join(SCRIPT_DIR, "..")

# ---------- helpers --------------------------------------------------------

def read_file(rel_path):
    """Read a file relative to the project root and return its content."""
    with open(os.path.join(PROJECT_ROOT, rel_path)) as f:
        return f.read()

def find_define(text, name):
    """Return the integer value of a #define, or None."""
    m = re.search(r'#define\s+' + name + r'\s+\(?\s*\(?\w*\)?\s*(\d+)', text)
    return int(m.group(1)) if m else None

def parse_tasks():
    """Extract task entries from the TimelineTaskConfig_t array in main.c."""
    content = read_file("src/main.c")
    tasks = []
    # find each { ... } block inside the array
    array_match = re.search(
        r'TimelineTaskConfig_t\s+\w+\[\]\s*=\s*\{(.+?)};',
        content, re.DOTALL)
    if not array_match:
        return tasks
    for block in re.finditer(r'\{([^}]+)\}', array_match.group(1)):
        fields = block.group(1)
        name  = re.search(r'\.pcTaskName\s*=\s*"([^"]+)"', fields)
        ttype = "HRT" if "HARD_RT" in fields else "SRT"
        start = re.search(r'\.ulStart_time\s*=\s*(\d+)', fields)
        end   = re.search(r'\.ulEnd_time\s*=\s*(\d+)',   fields)
        if name and start and end:
            tasks.append({
                "name":  name.group(1),
                "type":  ttype,
                "start": int(start.group(1)),
                "end":   int(end.group(1)),
            })
    return tasks

# ---------- tests ----------------------------------------------------------

def test_hrt_overlap(tasks):
    """No two HRT tasks should have overlapping time windows."""
    hrt = [t for t in tasks if t["type"] == "HRT"]
    for i, a in enumerate(hrt):
        for b in hrt[i+1:]:
            if a["start"] < b["end"] and b["start"] < a["end"]:
                return False, f'{a["name"]} overlaps {b["name"]}'
    return True, f"no overlaps among {len(hrt)} HRT tasks"

def test_timing(tasks):
    """Every HRT window must satisfy start < end <= MAJOR_FRAME_TICKS."""
    cfg = read_file("include/scheduler_config.h")
    major = find_define(cfg, "MAJOR_FRAME_TICKS") or 1000
    for t in tasks:
        if t["type"] != "HRT":
            continue
        if t["start"] >= t["end"]:
            return False, f'{t["name"]}: start >= end'
        if t["end"] > major:
            return False, f'{t["name"]}: end_time exceeds major frame ({major})'
    return True, "all HRT windows valid"

def test_freertos_config():
    """Check that key FreeRTOS settings have the expected values."""
    cfg = read_file("include/FreeRTOSConfig.h")
    checks = {
        "configUSE_PREEMPTION": 1,
        "configUSE_TIMERS":     1,
        "configTICK_RATE_HZ":   1000,
    }
    for name, expected in checks.items():
        val = find_define(cfg, name)
        if val != expected:
            return False, f"{name} = {val}, expected {expected}"
    return True, "configuration valid"

def test_build():
    """Run make clean && make all and check for errors."""
    subprocess.run(["make", "clean"], capture_output=True, cwd=PROJECT_ROOT)
    r = subprocess.run(["make", "all"], capture_output=True, text=True,
                       timeout=120, cwd=PROJECT_ROOT)
    if r.returncode != 0:
        return False, "build failed:\n" + r.stderr[-300:]
    elf = os.path.join(PROJECT_ROOT, "build", "scheduler.elf")
    if not os.path.isfile(elf):
        return False, "scheduler.elf not found"
    return True, "build successful"

def test_memory():
    """Check that FLASH and RAM usage fit within LM3S6965 limits."""
    elf = os.path.join(PROJECT_ROOT, "build", "scheduler.elf")
    r = subprocess.run(["arm-none-eabi-size", elf],
                       capture_output=True, text=True, timeout=5)
    if r.returncode != 0:
        return False, "arm-none-eabi-size failed"
    cols = r.stdout.strip().split("\n")[1].split()
    text, data, bss = int(cols[0]), int(cols[1]), int(cols[2])
    flash = text + data
    ram   = data + bss
    info  = f"FLASH {flash}/{256*1024}  RAM {ram}/{64*1024}"
    if flash > 256 * 1024 or ram > 64 * 1024:
        return False, "exceeds limits — " + info
    return True, info

# ---------- runtime helpers ------------------------------------------------

QEMU_CMD = ["qemu-system-arm", "-machine", "lm3s6965evb",
            "-kernel", os.path.join(PROJECT_ROOT, "build", "scheduler.elf"),
            "-serial", "file:/tmp/qemu_test.log",
            "-monitor", "none", "-nographic"]

def run_on_qemu(seconds=6):
    """Boot the scheduler on QEMU, let it run, return the serial output."""
    log = "/tmp/qemu_test.log"
    if os.path.exists(log):
        os.remove(log)
    proc = subprocess.Popen(QEMU_CMD, stdout=subprocess.DEVNULL,
                            stderr=subprocess.DEVNULL)
    time.sleep(seconds)
    proc.kill()
    proc.wait()
    if not os.path.isfile(log):
        return ""
    with open(log) as f:
        return f.read()

# ---------- runtime tests --------------------------------------------------

def test_hrt_activation(output, tasks):
    """Test 6: Every HRT task must be activated at least once per frame."""
    hrt_names = [t["name"] for t in tasks if t["type"] == "HRT"]
    for name in hrt_names:
        if f"[{name}] Activated" not in output:
            return False, f"{name} was never activated"
    return True, f"all HRT tasks activated ({', '.join(hrt_names)})"

def test_hrt_timing(output, tasks):
    """Test 7: HRT activation tick should be close to its configured start."""
    # Check first frame only (tick values = raw start offsets)
    for t in tasks:
        if t["type"] != "HRT":
            continue
        pattern = rf'\[{t["name"]}\] Activated at tick (\d+)'
        m = re.search(pattern, output)
        if not m:
            return False, f'{t["name"]} activation not found'
        tick = int(m.group(1))
        # Allow up to 5 ticks of jitter from configured start
        if abs(tick - t["start"]) > 5:
            return False, f'{t["name"]} activated at {tick}, expected ~{t["start"]}'
    return True, "HRT tasks activated at correct offsets"

def test_no_deadline_miss(output):
    """Test 8: Trace statistics must show 0 deadline misses."""
    m = re.search(r'Deadline Misses:\s+(\d+)', output)
    if not m:
        return False, "could not find deadline miss count in output"
    misses = int(m.group(1))
    if misses != 0:
        return False, f"{misses} deadline miss(es) detected"
    return True, "0 deadline misses"

def test_frame_reset(output):
    """Test 9: At least 2 frame resets must occur (proves cyclic repetition)."""
    resets = re.findall(r'\[FRAME_RESET\]', output)
    creates = re.findall(r'\[CREATE\]', output)
    if len(resets) < 2:
        return False, f"only {len(resets)} frame reset(s), expected >= 2"
    if len(creates) < 8:
        return False, f"only {len(creates)} task creates, expected >= 8 (4 tasks x 2 resets)"
    return True, f"{len(resets)} frame resets, tasks recreated each time"

def test_srt_execution(output):
    """Test 10: SRT tasks must execute during idle gaps."""
    srt_starts = re.findall(r'SRT_\w+ START', output)
    if len(srt_starts) == 0:
        return False, "no SRT task execution found in trace"
    return True, f"SRT tasks ran {len(srt_starts)} time(s)"

# ---------- main -----------------------------------------------------------

def main():
    print("=" * 50)
    print("TIMELINE SCHEDULER — TEST SUITE")
    print("=" * 50)

    tasks = parse_tasks()
    if not tasks:
        print("ERROR: could not parse tasks from main.c")
        sys.exit(1)

    print(f"\nFound {len(tasks)} tasks:")
    for t in tasks:
        print(f"  {t['name']:10s}  {t['type']}  [{t['start']}-{t['end']}]")

    # --- Part A: static checks ---
    static_tests = [
        ("HRT Overlap",     lambda: test_hrt_overlap(tasks)),
        ("Timing",          lambda: test_timing(tasks)),
        ("FreeRTOS Config", lambda: test_freertos_config()),
        ("Build",           lambda: test_build()),
        ("Memory",          lambda: test_memory()),
    ]

    print("\nPart A — Static checks\n")
    passed = 0
    for name, fn in static_tests:
        ok, msg = fn()
        status = "PASS" if ok else "FAIL"
        print(f"  [{status}] {name}: {msg}")
        passed += ok

    # --- Part B: runtime checks (need QEMU) ---
    print("\nPart B — Runtime checks (QEMU)\n")
    print("  Booting scheduler on QEMU (6 s) ...")
    output = run_on_qemu(seconds=6)
    if not output:
        print("  [SKIP] QEMU produced no output — is qemu-system-arm installed?")
        total = len(static_tests)
        print(f"\n{passed}/{total} tests passed (runtime skipped).")
        sys.exit(0 if passed == total else 1)

    runtime_tests = [
        ("HRT Activation",  lambda: test_hrt_activation(output, tasks)),
        ("HRT Timing",      lambda: test_hrt_timing(output, tasks)),
        ("No Deadline Miss", lambda: test_no_deadline_miss(output)),
        ("Frame Reset",     lambda: test_frame_reset(output)),
        ("SRT Execution",   lambda: test_srt_execution(output)),
    ]

    for name, fn in runtime_tests:
        ok, msg = fn()
        status = "PASS" if ok else "FAIL"
        print(f"  [{status}] {name}: {msg}")
        passed += ok

    total = len(static_tests) + len(runtime_tests)
    print(f"\n{passed}/{total} tests passed.")
    sys.exit(0 if passed == total else 1)

if __name__ == "__main__":
    main()
