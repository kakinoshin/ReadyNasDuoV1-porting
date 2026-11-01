CROSS	?= ../../toolchain/sparc-elf/bin/sparc-elf-
CC	= $(CROSS)gcc
READELF	= $(CROSS)readelf

CFLAGS	= -O2 -mcpu=v7 -mno-fpu -mno-app-regs -fno-pic -ffreestanding -fno-builtin \
	  -fno-stack-protector -fno-asynchronous-unwind-tables -fno-common \
	  -fno-tree-loop-distribute-patterns -Wall -Wextra

IMAGE	?= ../probe/probe-boot.bin

all: padre_regs.ko padre_go.ko check

padre_regs.ko: padre_regs.c
	$(CC) $(CFLAGS) -c -o $@ $<

padre_go.ko: padre_go.c $(IMAGE)
	$(CC) $(CFLAGS) -Wa,-Av8 -DIMAGE_FILE='"$(IMAGE)"' -c -o $@ $<

# arch/padre/kernel/module.c handles only these relocation types.
check: padre_regs.ko padre_go.ko
	! for m in $^; do $(READELF) -rW $$m; done | awk '/R_SPARC_/ { print $$3 }' | sort -u | \
	    grep -vE '^R_SPARC_(32|WDISP30|WDISP22|LO10|HI22)$$'

clean:
	rm -f padre_regs.ko padre_go.ko

.PHONY: all check clean
