CROSS	?= ../../toolchain/sparc-elf/bin/sparc-elf-
CC	= $(CROSS)gcc
OBJCOPY	= $(CROSS)objcopy
OBJDUMP	= $(CROSS)objdump

# C stays V7; only hw.S may contain V8 multiply/divide (-Av8 for gas).
CFLAGS	= -O2 -mcpu=v7 -mno-fpu -ffreestanding -fno-builtin -Wall -Wextra
ASFLAGS	= -Wa,-Av8
LDFLAGS	= -nostdlib -static -T link.ld -Wl,--build-id=none

all: probe.bin probe-boot.bin irqtest-boot.bin mmutest-boot.bin probe-user check

probe.elf: start.S regs.S hw.S probe.c link.ld
	$(CC) $(CFLAGS) $(ASFLAGS) $(LDFLAGS) -o $@ start.S regs.S hw.S probe.c -lgcc

# Self-contained image with its own trap table, started by hw/kmod/padre_go.ko.
probe-boot.elf: boot.S regs.S hw.S probe.c link-boot.ld
	$(CC) $(CFLAGS) -DPROBE_TRAP $(ASFLAGS) -nostdlib -static -T link-boot.ld \
	    -Wl,--build-id=none -o $@ boot.S regs.S hw.S probe.c -lgcc

probe-boot.bin: probe-boot.elf
	$(OBJCOPY) -O binary $< $@

irqtest-boot.elf: boot.S irqtrap.S regs.S irqtest.c link-boot.ld
	$(CC) $(CFLAGS) -DIRQ_TRAPS $(ASFLAGS) -nostdlib -static -T link-boot.ld \
	    -Wl,--build-id=none -o $@ boot.S irqtrap.S regs.S irqtest.c -lgcc

irqtest-boot.bin: irqtest-boot.elf
	$(OBJCOPY) -O binary $< $@

mmutest-boot.elf: boot.S mmutrap.S mmutest.c link-boot.ld
	$(CC) $(CFLAGS) -DMMU_TRAPS $(ASFLAGS) -nostdlib -static -T link-boot.ld \
	    -Wl,--build-id=none -o $@ boot.S mmutrap.S mmutest.c -lgcc

mmutest-boot.bin: mmutest-boot.elf
	$(OBJCOPY) -O binary $< $@

# Static Linux binary for RAIDiator. The kernel only needs ET_EXEC EM_SPARC.
probe-user: user.S hw.S probe.c link-user.ld
	$(CC) $(CFLAGS) -DUSERLAND $(ASFLAGS) -nostdlib -static -T link-user.ld \
	    -Wl,--build-id=none -o $@ user.S hw.S probe.c -lgcc

probe.bin: probe.elf
	$(OBJCOPY) -O binary $< $@

# Fails if the compiler put V8 mul/div into the V7 reference code.
check: probe.elf probe-boot.elf irqtest-boot.elf mmutest-boot.elf probe-user
	$(OBJDUMP) -d probe.elf | sh check-v7.sh
	$(OBJDUMP) -d probe-boot.elf | sh check-v7.sh
	$(OBJDUMP) -d irqtest-boot.elf | sh check-v7.sh
	$(OBJDUMP) -d mmutest-boot.elf | sh check-v7.sh
	$(OBJDUMP) -d probe-user | sh check-v7.sh

clean:
	rm -f probe.elf probe.bin probe-boot.elf probe-boot.bin irqtest-boot.elf \
	    irqtest-boot.bin mmutest-boot.elf mmutest-boot.bin probe-user

.PHONY: all check clean
