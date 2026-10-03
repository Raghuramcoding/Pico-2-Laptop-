# Run it in a virtual machine (no board needed)

The VM uses **QEMU**, a free emulator that can pretend to be an Arm Cortex-M33
chip. It runs your real OS: scheduler, heap, FAT driver, terminal and shell.

## What it tests, and what it does not
| Tested in the VM | NOT tested in the VM (needs the real Pico 2) |
|---|---|
| Scheduler, sleeping, task switching | Clocks / crystal / PLL |
| Heap, shell commands, paths | Real UART, SPI, GPIO pins |
| FAT16/FAT32 reading, `ls` `cd` `cat` | The ILI9341 display driver |
| The 40x30 text terminal and font | The SD card protocol |
| Your own new OS code and commands | The key-matrix scanner (wiring) |
| | The BIOS itself and the boot block |

In the VM the BIOS is a small stand-in. That is why you can write and debug OS
features (editor, programs, new commands) before any hardware arrives.

## 1. Install QEMU
- Windows: install QEMU from qemu.org. Python 3 is also needed (python.org).
- macOS: `brew install qemu`
- Linux / WSL: `sudo apt install qemu-system-arm`
- No other tools are needed for the prebuilt VM.

## 2. Run it
From the project folder:
```
python3 tools/run_vm.py            # text only: type in this terminal
python3 tools/run_vm.py --screen   # plus a window showing the 320x240 LCD
```
(On Windows use `python` instead of `python3`.) Quit with **Ctrl-]**.
For `--screen` on Linux you may need `sudo apt install python3-tk`.
In the window you can type directly. It acts like the laptop keyboard.

Try: `help`, `mount`, `ls`, `cd docs`, `cat hello.txt`, `ps`, `mem`.

## 3. Your own files on the virtual SD card
```
python3 tools/make_disk.py my_files_folder mydisk.img      # needs dosfstools + mtools (Linux/WSL)
python3 tools/run_vm.py --disk mydisk.img
```
Use short 8.3 names like `NOTES.TXT`. The image is at most 16 MB.

## 4. Automatic runs and screenshots
```
python3 tools/run_vm.py --script "help;mount;ls;cat readme.txt" --snapshot screen.png
```

## 5. Build the VM version yourself (after you change the OS code)
```
cd firmware
make vm              # builds build/vm_os.elf and starts it
make qemu-test       # automatic checks (needs qemu-system-arm)
make test            # PC-only unit tests
```
The launcher uses `prebuilt/vm/pico2os_vm.elf` if it exists, otherwise
`firmware/build/vm_os.elf`. After you change the OS, either run
`python3 tools/run_vm.py --elf firmware/build/vm_os.elf` or copy that file over
the prebuilt one.

## 6. Debugging with GDB (optional)
Add `-s -S` to the QEMU command in `tools/run_vm.py`, then in another window:
```
arm-none-eabi-gdb firmware/build/vm_os.elf -ex "target remote :1234"
```
You can set breakpoints in `sched.c` or `shell.c`. This is the big benefit of
the VM: single-step your own OS.

## Want to simulate the real BIOS too?
QEMU does not have an RP2350 chip model, so the real BIOS cannot run here.
Online simulators such as Wokwi might support the Pico 2 together with a
display and SD card. I could not confirm that, so check their current
board list before relying on it.
