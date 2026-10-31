CROSS	?= ../../toolchain/sparc-elf/bin/sparc-elf-
CC	= $(CROSS)gcc
OBJCOPY	= $(CROSS)objcopy
OBJDUMP	= $(CROSS)objdump

# C stays V7; only start.S may contain V8 multiply/divide (-Av8 for gas).
CFLAGS	= -O2 -mcpu=v7 -mno-fpu -ffreestanding -fno-builtin -Wall -Wextra
ASFLAGS	= -Wa,-Av8
LDFLAGS	= -nostdlib -static -T link.ld -Wl,--build-id=none

all: probe.bin check

probe.elf: start.S probe.c link.ld
	$(CC) $(CFLAGS) $(ASFLAGS) $(LDFLAGS) -o $@ start.S probe.c -lgcc

probe.bin: probe.elf
	$(OBJCOPY) -O binary $< $@

# Fails if the compiler put V8 mul/div into the V7 reference code.
check: probe.elf
	$(OBJDUMP) -d $< | sh check-v7.sh

clean:
	rm -f probe.elf probe.bin

.PHONY: all check clean
