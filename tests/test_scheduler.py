#!/usr/bin/env python3
"""
Timeline Scheduler - Automated Test Suite
Author: Stefano (QA & Environment Lead)
Date: January 14, 2026
"""

import sys
import os
import re
import subprocess
from pathlib import Path
from dataclasses import dataclass
from typing import List

@dataclass
class Task:
    name: str
    type: str
    start_time: int
    end_time: int
    subframe_id: int
    stack_depth: int

@dataclass
class TestResult:
    test_name: str
    passed: bool
    message: str
    details: str = ""

class TestSuite:
    def __init__(self, project_root: str = "."):
        self.project_root = Path(project_root)
        self.tasks: List[Task] = []
        self.test_results: List[TestResult] = []
        self.major_frame_ticks = 1000
        self.config_data = {}
        
    def parse_configuration(self) -> bool:
        """Parse scheduler_config.h"""
        config_file = self.project_root / "include" / "scheduler_config.h"
        try:
            with open(config_file, 'r') as f:
                content = f.read()
            match = re.search(r'#define\s+MAJOR_FRAME_TICKS\s+(\d+)', content)
            if match:
                self.major_frame_ticks = int(match.group(1))
            return True
        except Exception as e:
            print(f"[ERROR] Failed to parse config: {e}")
            return False
    
    def parse_freertos_config(self) -> bool:
        """Parse FreeRTOSConfig.h"""
        config_file = self.project_root / "include" / "FreeRTOSConfig.h"
        try:
            with open(config_file, 'r') as f:
                content = f.read()
            
            patterns = {
                'configUSE_PREEMPTION': r'#define\s+configUSE_PREEMPTION\s+(\d+)',
                'configUSE_TIMERS': r'#define\s+configUSE_TIMERS\s+(\d+)',
                'configTICK_RATE_HZ': r'#define\s+configTICK_RATE_HZ\s+\(\s*\(TickType_t\)\s*(\d+)',
            }
            
            for key, pattern in patterns.items():
                match = re.search(pattern, content)
                if match:
                    self.config_data[key] = int(match.group(1))
            
            return True
        except Exception as e:
            print(f"[ERROR] Failed to parse FreeRTOS config: {e}")
            return False
    
    def parse_timeline_config(self) -> bool:
        """Parse main.c to extract tasks"""
        main_file = self.project_root / "src" / "main.c"
        try:
            with open(main_file, 'r', encoding='utf-8') as f:
                lines = f.readlines()
            
            in_array = False
            current = {}
            
            for line in lines:
                line = line.strip()
                
                if 'TimelineTaskConfig_t' in line and '[]' in line:
                    in_array = True
                    continue
                
                if not in_array:
                    continue
                
                if '.pcTaskName' in line:
                    match = re.search(r'"([^"]+)"', line)
                    if match:
                        current['name'] = match.group(1)
                
                elif '.xType' in line:
                    current['type'] = 'HARD_RT' if 'HARD_RT' in line else 'SOFT_RT'
                
                elif '.ulStart_time' in line:
                    match = re.search(r'=\s*(\d+)', line)
                    if match:
                        current['start'] = int(match.group(1))
                
                elif '.ulEnd_time' in line:
                    match = re.search(r'=\s*(\d+)', line)
                    if match:
                        current['end'] = int(match.group(1))
                
                elif '.ulSubframe_id' in line:
                    match = re.search(r'=\s*(\d+)', line)
                    if match:
                        current['subframe'] = int(match.group(1))
                
                elif '.usStackDepth' in line:
                    current['stack'] = 128  # Default
                
                if '}' in line and 'name' in current:
                    task = Task(
                        name=current['name'],
                        type=current['type'],
                        start_time=current.get('start', 0),
                        end_time=current.get('end', 0),
                        subframe_id=current.get('subframe', 0),
                        stack_depth=current.get('stack', 128)
                    )
                    self.tasks.append(task)
                    print(f"  ✓ {task.name} ({task.type}) [{task.start_time}-{task.end_time}]")
                    current = {}
                
                if in_array and '};' in line:
                    break
            
            return len(self.tasks) > 0
            
        except Exception as e:
            print(f"[ERROR] Failed to parse tasks: {e}")
            return False
    
    def test_hrt_overlap(self) -> TestResult:
        """Test 1: Check HRT task overlaps"""
        hrt = [t for t in self.tasks if t.type == "HARD_RT"]
        
        if len(hrt) < 2:
            return TestResult("HRT Overlap", True, "No overlap possible")
        
        overlaps = []
        for i, t1 in enumerate(hrt):
            for t2 in hrt[i+1:]:
                if t1.start_time < t2.end_time and t2.start_time < t1.end_time:
                    overlaps.append(f"{t1.name} overlaps {t2.name}")
        
        if overlaps:
            return TestResult("HRT Overlap", False, 
                            f"Found {len(overlaps)} overlap(s)", 
                            "\n".join(overlaps))
        
        return TestResult("HRT Overlap", True, 
                         f"All {len(hrt)} HRT tasks OK")
    
    def test_timing(self) -> TestResult:
        """Test 2: Verify timing consistency"""
        issues = []
        
        for task in self.tasks:
            if task.type == "HARD_RT":
                if task.start_time >= task.end_time:
                    issues.append(f"{task.name}: Invalid window [{task.start_time}-{task.end_time}]")
                
                if task.end_time > self.major_frame_ticks:
                    issues.append(f"{task.name}: Exceeds major frame")
        
        if issues:
            return TestResult("Timing", False, 
                            f"Found {len(issues)} issue(s)", 
                            "\n".join(issues))
        
        return TestResult("Timing", True, "All tasks valid")
    
    def test_freertos_config(self) -> TestResult:
        """Test 3: Validate FreeRTOS settings"""
        required = {
            'configUSE_PREEMPTION': 1,
            'configUSE_TIMERS': 1,
            'configTICK_RATE_HZ': 1000,
        }
        
        issues = []
        for key, expected in required.items():
            if self.config_data.get(key) != expected:
                issues.append(f"{key} = {self.config_data.get(key)}, expected {expected}")
        
        if issues:
            return TestResult("FreeRTOS Config", False, 
                            "Configuration mismatch", 
                            "\n".join(issues))
        
        return TestResult("FreeRTOS Config", True, "Configuration valid")
    
    def test_build(self) -> TestResult:
        """Test 4: Build the project"""
        try:
            subprocess.run(['make', 'clean'], 
                         capture_output=True, timeout=10, 
                         cwd=self.project_root)
            
            result = subprocess.run(['make', 'all'], 
                                  capture_output=True, text=True, 
                                  timeout=120, cwd=self.project_root)
            
            if result.returncode != 0:
                return TestResult("Build", False, "Build failed", 
                                result.stderr[-500:])
            
            elf = self.project_root / "build" / "scheduler.elf"
            if not elf.exists():
                return TestResult("Build", False, "ELF not found")
            
            return TestResult("Build", True, "Build successful")
            
        except subprocess.TimeoutExpired:
            return TestResult("Build", False, "Build timeout")
        except Exception as e:
            return TestResult("Build", False, str(e))
    
    def test_memory(self) -> TestResult:
        """Test 5: Check memory usage"""
        try:
            elf = self.project_root / "build" / "scheduler.elf"
            result = subprocess.run(['arm-none-eabi-size', str(elf)],
                                  capture_output=True, text=True, timeout=5)
            
            if result.returncode != 0:
                return TestResult("Memory", False, "Size check failed")
            
            lines = result.stdout.strip().split('\n')
            if len(lines) < 2:
                return TestResult("Memory", False, "Invalid output")
            
            values = lines[1].split()
            text = int(values[0])
            data = int(values[1])
            bss = int(values[2])
            
            flash_used = text + data
            ram_used = data + bss
            flash_limit = 256 * 1024
            ram_limit = 64 * 1024
            
            details = f"FLASH: {flash_used:,}/{flash_limit:,} ({flash_used/flash_limit*100:.1f}%)\n"
            details += f"RAM: {ram_used:,}/{ram_limit:,} ({ram_used/ram_limit*100:.1f}%)"
            
            if flash_used > flash_limit or ram_used > ram_limit:
                return TestResult("Memory", False, "Exceeds limits", details)
            
            return TestResult("Memory", True, "Within limits", details)
            
        except Exception as e:
            return TestResult("Memory", False, str(e))
    
    def run_all_tests(self) -> bool:
        """Run all tests"""
        print("\n" + "="*60)
        print("TIMELINE SCHEDULER - AUTOMATED TEST SUITE")
        print("Author: Stefano (QA & Environment Lead)")
        print("="*60 + "\n")
        
        print("[1/3] Parsing configurations...")
        if not self.parse_configuration():
            return False
        if not self.parse_freertos_config():
            return False
        
        print(f"\n[2/3] Parsing tasks from main.c...")
        if not self.parse_timeline_config():
            return False
        
        print(f"\n[3/3] Running tests...")
        self.test_results = [
            self.test_hrt_overlap(),
            self.test_timing(),
            self.test_freertos_config(),
            self.test_build(),
            self.test_memory(),
        ]
        
        print("\n" + "="*60)
        print("TEST RESULTS")
        print("="*60)
        
        passed = failed = 0
        for r in self.test_results:
            status = "✅ PASS" if r.passed else "❌ FAIL"
            print(f"\n{status} {r.test_name}")
            print(f"  → {r.message}")
            if r.details:
                for line in r.details.split('\n'):
                    print(f"    {line}")
            
            if r.passed:
                passed += 1
            else:
                failed += 1
        
        print("\n" + "="*60)
        print(f"SUMMARY: {passed}/{passed+failed} passed")
        print("="*60 + "\n")
        
        return failed == 0

def main():
    script_dir = Path(__file__).parent
    project_root = script_dir.parent if script_dir.name == "tests" else script_dir
    
    suite = TestSuite(project_root=str(project_root))
    success = suite.run_all_tests()
    
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()
