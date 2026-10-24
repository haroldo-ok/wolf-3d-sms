# SPDX-License-Identifier: MIT -- Copyright (c) 2026 Haroldo-OK. See LICENSE.
# Headless SMS emulator for automated playtesting. Builds a single `smstest`
# binary with the host C compiler (no SMS toolchain needed).
CFLAGS := -O2 -w
all: smstest
smstest: smstest.c z80.c z80.h
	$(CC) $(CFLAGS) -o $@ smstest.c z80.c
clean:
	rm -f smstest
.PHONY: all clean
