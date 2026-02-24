#!/usr/bin/env python3
"""
Timeline Scheduler - Test Suite
Author: Stefano (QA & Environment Lead)
Date: January 14, 2026

Runs 5 static checks on the scheduler configuration:
  1. HRT overlap       – no two HRT tasks share the same time window
  2. Timing validity   – every HRT window is well-formed and inside the major frame
  3. FreeRTOS config   – key kernel settings match expected values
  4. Build             – the project compiles without errors
  5. Memory            – FLASH/RAM usage stays within LM3S6965 limits
"""

import sys, re, subprocess, os

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

    tests = [
        ("HRT Overlap",    lambda: test_hrt_overlap(tasks)),
        ("Timing",         lambda: test_timing(tasks)),
        ("FreeRTOS Config", lambda: test_freertos_config()),
        ("Build",          lambda: test_build()),
        ("Memory",         lambda: test_memory()),
    ]

    print("\nRunning tests ...\n")
    passed = 0
    for name, fn in tests:
        ok, msg = fn()
        status = "PASS" if ok else "FAIL"
        print(f"  [{status}] {name}: {msg}")
        passed += ok

    print(f"\n{passed}/{len(tests)} tests passed.")
    sys.exit(0 if passed == len(tests) else 1)

if __name__ == "__main__":
    main()
