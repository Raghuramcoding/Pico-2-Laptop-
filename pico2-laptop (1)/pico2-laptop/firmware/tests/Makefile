CC ?= gcc
SRC = ../common/kprintf.c ../common/kbd_core.c ../os/mem.c ../os/fat.c ../os/shell.c ../os/pathutil.c
CFLAGS = -std=gnu11 -O1 -g -Wall -Wextra -Wno-unused-parameter -fsanitize=address,undefined \
         -I../common -I../os

run: test_all
	python3 run_tests.py

test_all: test_all.c $(SRC)
	$(CC) $(CFLAGS) test_all.c $(SRC) -o $@

clean:
	rm -rf test_all images
.PHONY: run clean
