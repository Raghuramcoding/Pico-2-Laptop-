#!/usr/bin/env python3
"""Boots the OS in QEMU (Cortex-M33), types shell commands at it over the
serial port and checks the answers.  Proves the scheduler / context switch /
shell work on a real M33 core model while a task hogs the CPU."""
import subprocess, sys, time, os

elf = sys.argv[1]
cmds = ["help", "echo hello world", "uptime", "mem", "ps", "info", "pwd", "ls", "peek 10000000 2"]
p = subprocess.Popen(["qemu-system-arm", "-M", "mps2-an505", "-cpu", "cortex-m33", "-nographic",
                      "-serial", "stdio", "-monitor", "none", "-kernel", elf],
                     stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
time.sleep(1.5)
for c in cmds:
    p.stdin.write((c + "\r").encode()); p.stdin.flush()
    time.sleep(0.4)
time.sleep(1.0)
p.kill()
out = p.stdout.read().decode(errors="replace").replace("\r", "")
print(out)
checks = [
    ("OS banner", "PicoOS 0.1"),
    ("shell prompt", "/> "),
    ("help lists commands", "reboot"),
    ("echo works", "hello world"),
    ("uptime works", "up 0:00:"),
    ("heap stats", "heap used"),
    ("ps shows tasks", "shell"),
    ("preempted busy task shows in ps", "spin"),
    ("sleeping task woke 5 times", "[counter] tick 5"),
    ("info works", "RAM for OS"),
    ("no sd message handled", "No file system"),
    ("peek reads vector table", "10000000:"),
]
bad = 0
for name, needle in checks:
    ok = needle in out
    print(("PASS " if ok else "FAIL ") + name)
    bad += not ok
sys.exit(1 if bad else 0)
