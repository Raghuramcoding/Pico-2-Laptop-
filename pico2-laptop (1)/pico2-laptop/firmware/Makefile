# Pico 2 laptop firmware.  Needs: arm-none-eabi-gcc, python3, make.
#   make phase0   -> build/phase0.uf2   (blink + UART hello)
#   make          -> build/laptop.uf2   (BIOS + OS together)
#   make test     -> run the host-side unit tests (needs gcc, mtools, dosfstools)
CROSS  ?= arm-none-eabi-
CC      = $(CROSS)gcc
OBJCOPY = $(CROSS)objcopy
SIZE    = $(CROSS)size
B       = build

ARCH   = -mcpu=cortex-m33 -mthumb -mfloat-abi=soft
CFLAGS = $(ARCH) -Os -g -std=gnu11 -ffreestanding -fno-tree-loop-distribute-patterns \
         -ffunction-sections -fdata-sections -Wall -Wextra -Wno-unused-parameter \
         -Icommon -Ibios -Ios
LDFLAGS = $(ARCH) -nostdlib -nostartfiles -Wl,--gc-sections -Wl,-Map=$(@:.elf=.map)

COMMON_SRC = common/hal.c common/kprintf.c common/lib.c
BIOS_SRC   = bios/bios_main.c bios/lcd.c bios/sd.c bios/kbd.c bios/api.c common/kbd_core.c $(COMMON_SRC)
OS_SRC     = os/os_main.c os/mem.c os/sched.c os/fat.c os/shell.c os/term.c os/pathutil.c \
             common/kbd_core.c common/kprintf.c common/lib.c

all: $(B)/laptop.uf2

$(B):
	mkdir -p $(B)

$(B)/phase0.elf: phase0/phase0.c common/startup.c $(COMMON_SRC) phase0/phase0.ld | $(B)
	$(CC) $(CFLAGS) -DWITH_IMAGE_DEF phase0/phase0.c common/startup.c $(COMMON_SRC) \
	  $(LDFLAGS) -Tphase0/phase0.ld -lgcc -o $@
	$(SIZE) $@

$(B)/bios.elf: $(BIOS_SRC) common/startup.c bios/bios.ld | $(B)
	$(CC) $(CFLAGS) -DWITH_IMAGE_DEF $(BIOS_SRC) common/startup.c \
	  $(LDFLAGS) -Tbios/bios.ld -lgcc -o $@
	$(SIZE) $@

$(B)/os.elf: $(OS_SRC) common/startup.c os/os.ld | $(B)
	$(CC) $(CFLAGS) $(OS_SRC) common/startup.c \
	  $(LDFLAGS) -Tos/os.ld -lgcc -o $@
	$(SIZE) $@

$(B)/%.bin: $(B)/%.elf
	$(OBJCOPY) -O binary $< $@

$(B)/phase0.uf2: $(B)/phase0.bin
	python3 ../tools/mkuf2.py -o $@ 0x10000000:$<

phase0: $(B)/phase0.uf2

$(B)/laptop.uf2: $(B)/bios.bin $(B)/os.bin
	python3 ../tools/mkuf2.py -o $@ 0x10000000:$(B)/bios.bin 0x10040000:$(B)/os.bin
	python3 ../tools/mkuf2.py -o $(B)/bios_only.uf2 0x10000000:$(B)/bios.bin
	python3 ../tools/mkuf2.py -o $(B)/os_only.uf2 0x10040000:$(B)/os.bin

test:
	$(MAKE) -C tests run

# Run the real OS (scheduler, shell) on an emulated Cortex-M33.  Needs qemu-system-arm.
$(B)/qemu_os.elf: $(OS_SRC) qemu/qemu_stub.c common/startup.c qemu/qemu.ld | $(B)
	$(CC) $(CFLAGS) -DVM_DEMO_TASKS $(OS_SRC) qemu/qemu_stub.c common/startup.c \
	  $(LDFLAGS) -Tqemu/qemu.ld -lgcc -o $@

# the version people run: same OS, without the extra demo tasks
$(B)/vm_os.elf: $(OS_SRC) qemu/qemu_stub.c common/startup.c qemu/qemu.ld | $(B)
	$(CC) $(CFLAGS) $(OS_SRC) qemu/qemu_stub.c common/startup.c \
	  $(LDFLAGS) -Tqemu/qemu.ld -lgcc -o $@

qemu-test: $(B)/qemu_os.elf
	python3 qemu/run_qemu_test.py $(B)/qemu_os.elf

vm: $(B)/vm_os.elf
	python3 ../tools/run_vm.py $(B)/vm_os.elf

clean:
	rm -rf $(B)
	$(MAKE) -C tests clean

.PHONY: all phase0 test clean qemu-test vm
