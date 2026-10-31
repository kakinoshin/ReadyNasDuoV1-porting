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
| M1 | A way to run the probe | `hw/probe` output appears on serial | Yes |
| M2 | Minimal kernel | `locore`, UART console, timer, INTC; prints the copyright banner | Yes |
| M3 | pmap | New `pmap`, `uvm` starts, `init` runs from a RAM disk | Yes |
| M4 | Userland | `sh` runs from a RAM disk | Yes |
| M5 | PCI and USB | `pcic`, `uhci`, `ehci`, boot from USB storage | Yes |
| M6 | Ethernet | Reverse engineer `padre_gmac`, NFS root | Yes |
| M7 | SATA | Analyse LP and DJ, attach `wd` | Yes |

USB comes before Ethernet because the VIA controllers are standard: existing drivers give
us storage without any custom IP analysis.

## Risks and open items

1. **Boot path (the main M1 obstacle).** iboot wants encrypted images, and `rn_nops` only
   fits iboot 1.00a030. Candidates:
   - Inspect the Duo's iboot and apply the same patching method.
   - Find `readynas_crypto` (by Henrik Nordström, named in the DIET README) and use its keys.
     No download location is known yet.
   - Start a kernel kexec-style from a Linux module on RAIDiator. With root SSH this avoids
     touching flash and is the safest, but the module must be built with the gcc 3.3 series.
2. **Serial.** M1 onward needs a 3.3 V TTL UART connection.
3. **No emulator.** QEMU has no padre model, which limits what can be checked without
   hardware. A minimal emulator may be worth building for M2 onward.
4. **A/D bits, the TLB walker and unexpected TLB traps.** These shape the pmap design and
   should be measured from a kernel module or bare metal before M3.
5. **LP dependence.** If SATA cannot run without LP, the firmware must be part of the port.
6. **No NAND writes.** They risk bricking the unit, and recovery options for iboot are limited.
