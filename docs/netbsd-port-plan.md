# NetBSD port plan

Based on [IT3107-analysis.md](IT3107-analysis.md).

## Approach

**Create a new kernel port, `sys/arch/padre`,** with `MACHINE=padre` and `MACHINE_ARCH=sparc`.

Reasons for not extending `sys/arch/sparc`:

- `pmap.c` (8,087 lines) covers sun4/sun4c/SRMMU only and does not fit the IT3107 MMU.
- `autoconf.c` and `promlib.c` assume OpenPROM. iboot has no OpenPROM.
- `CPU_ISSUN4M`-style branches are spread across 17 files.

The userland is the existing NetBSD/sparc. `.mul`/`.div` in `lib/libc/arch/sparc` are
software implementations (`umul.S`, `divrem.m4`), and `sparc--netbsdelf-gcc` was confirmed
to emit calls to them. An `objdump` scan of the full distribution comes after a userland build.

**`machdep.cpu_arch` must always return 7.** With 8 or 9, `etc/etc.sparc/ld.so.conf` loads
`libsparc_v8.so.0`, which swaps in hardware multiply/divide.
OpenSSL's `bn-sparcv8.S` is limited to sparc64 according to the comment in `bn.inc`;
confirm against the sparc `libcrypto` after a userland build.

Window handling, traps and signals in `locore.s` start from `sys/arch/sparc`. `msiiep.c`
and `MRCOFFEE` are the precedent for a PCI-integrated SoC.

## Traps in existing code

- `locore.s:4506-4509` patches `.mul`/`.umul`/`.div`/`.udiv` to hardware instructions at
  run time on V8 CPUs. **padre must remove this.**
- `-Wa,-Av8` in the kernel `CFLAGS` only permits the assembler, which is fine, but V8
  instructions must be scanned for in CI the way `check-v7.sh` does.
- `KERNBASE` is `0xf0000000`; on padre it is `0x80000000`. User space is only 1 GiB, so
  `VM_MAXUSER_ADDRESS`, `USRSTACK` and `MAXDSIZ` need review.
- `flush` is emulated from the illegal instruction trap. The `ld.elf_so` PLT code, signal
  trampolines and GCC trampolines depend on it.
- **`spl` cannot use PSR.PIL.** The field is a single global-disable bit, so IPL levels must
  be built in software from the INTC masks, for example a per-IPL `MASK_H`/`MASK_L` table.
  `splhigh` can still use the PIL bit. Every mask write needs the bug 505 dummy write.
  Trap levels are fixed per line and cannot be programmed, so the IPL of a line comes from
  the table, not from the trap level. The `sparc` port's `ipl.h` and softint scheme, which
  rely on real PIL levels and the sun4m soft interrupt register, do not carry over.
- **Kernel stacks must stay in the direct map.** The window handlers run with traps off, and
  a TLB miss there resets the CPU. Addresses at `0xf8000000` and above can miss.
- NAND access turns PCI off (`PCI_FEATURE_PCION`), and doing so under a running EHCI
  controller killed it on RAIDiator. A NetBSD NAND driver must quiesce PCI/USB first, or
  the port must leave NAND alone at run time.
- KVA is just under 128 MiB at `0xf8000000-0xffff0000`. `kernel_map` goes there, with the
  same entries shared in every process PGD, as Linux does.

## Development environment

| Purpose | Tool | Location |
|---|---|---|
| Bare-metal checks | GCC 16.2.0 / binutils 2.47 (`sparc-elf`) | `toolchain/sparc-elf` |
| NetBSD kernel and userland | NetBSD trunk `build.sh` (GCC 14 / binutils 2.46) | `src/netbsd-tools` |

## Milestones

| # | Work | Done when | Hardware |
|---|---|---|---|
| M0 | Toolchain | Cross compilers work | No |
| M1 | A way to run the probe | **Done:** `padre_go.ko` starts `probe-boot.bin` on bare metal | Yes |
| M2 | Minimal kernel | **Done:** threads switch, 100 Hz timer interrupts drive `kpause`, kernel reaches the root device prompt | Yes |
| M3 | pmap | New `pmap`, `uvm` starts, `init` runs from a RAM disk | Yes |
| M4 | Userland | `sh` runs from a RAM disk | Yes |
| M5 | PCI and USB | `pcic`, `uhci`, `ehci`, boot from USB storage | Yes |
| M6 | Ethernet | Reverse engineer `padre_gmac`, NFS root | Yes |
| M7 | SATA | Analyse LP and DJ, attach `wd` | Yes |

USB comes before Ethernet because the VIA controllers are standard: existing drivers give
us storage without any custom IP analysis.

## Risks and open items

`tools/build-kernel.sh` builds the kernel and `padre_go.ko` around it. `build.sh` does not
know `MACHINE=padre` yet.

1. **Boot path.** For development this is solved: `hw/kmod/padre_go.ko` enters an image from
   RAIDiator without touching flash, and GCC 16 builds the module, so the gcc 3.3 series is
   not needed. Each run needs a power cycle. This needs RAIDiator to stay on the disks, so
   a standalone boot still depends on iboot, whose image encryption is unsolved
   (`rn_nops` only fits iboot 1.00a030, and the Duo has 1.00a043).
2. **Serial.** M1 onward needs a 3.3 V TTL UART connection.
3. **No emulator.** QEMU has no padre model, which limits what can be checked without
   hardware. A minimal emulator may be worth building for M2 onward.
4. **A/D bits, the TLB walker and unexpected TLB traps.** These shape the pmap design and
   should be measured from a kernel module or bare metal before M3.
5. **LP dependence.** If SATA cannot run without LP, the firmware must be part of the port.
6. **Loading late after boot.** `padre_go.ko` overwrites `0x90000000` without asking Linux. The
   one run 4 hours after boot left Linux wedged, and runs right after boot all worked. The
   cause is not proven, so load it soon after boot.
7. **Unexplained Linux panic.** One NULL dereference in an interrupt handler at boot, before
   any module was loaded, after a run that ended in a power pull. It has not recurred.
8. **No NAND writes.** They risk bricking the unit, and recovery options for iboot are limited.
9. **No timecounter.** The kernel still uses the MI dummy counter, and `delay()` is an
   uncalibrated loop. Neither is accurate until a free-running counter is found.
10. **Software `spl` is lightly tested.** So far only the timer interrupts; a device at a
    different IPL is the first real check of the mask tables.
