# Infrant IT3107 (Padre / NEON) hardware analysis

Primary sources, from the NETGEAR GPL archive `RNR4_RND4_RND2_4.1.6_WW_src.zip`:
`linux-2.6.17.8/arch/padre`, `include/asm-padre`, `kernel_modules/` and
`glibc-2.3.2.ds1/debian/patches/infrant.dpatch`.
The official URL returns 404, so the archive comes from the Wayback Machine. The parts
used here are in `refs/gpl/extracted/`.

Confidence labels:
**[confirmed]** stated in source / **[inferred]** reasoned from source, not checked on
hardware / **[unknown]** not yet determined.
Unless listed under "Observed on the target unit", nothing has been checked on hardware.

## Observed on the target unit

From the serial console of one ReadyNAS Duo v1 during a normal boot. This shows what the
stock firmware reports; the instruction-level probe has not run yet.

| Item | Observed |
|---|---|
| Firmware | RAIDiator 4.1.16 |
| Boot loader | iboot 1.00a043, built 2008-10-01 (**not** the a030 that `rn_nops` needs) |
| Board | `bid = 01`, `BOARDID=1`, `ECC ON` |
| RAM | **1 GiB** (`Memory: 1010048k/1048576k`), not the stock 256 MiB |
| Console | 9600 8N1, ttyS0 at IRQ 7, login prompt on serial |
| NAND | Samsung 64 MiB, `0x00000000-0x00100000` and `0x00100000-0x04000000`, 0 bad blocks |
| Ethernet PHY | ID `0x0141, 0x0c` (Marvell OUI), linked at 1000 Mbps |
| Disks | 2 SATA on `ide1`/`ide2` at IRQ 32/33, DMA on channels 0/1, X-RAID |
| LP | Disk status reports `LP_S`; `Padre NSP IO` scans SATA at load |
| PCI/USB | No USB controller probe lines in the kernel log before hotplug |

Sending `i` at 9600 baud right after `Loading ...` stops at the `iboot>` prompt on the Duo,
with this help text. The commands have not been tried and their effects are unknown:

```
tftp MAC IP IPsvr file mem
nand
usb
ics v12
ICS reg val
pci v17
reason x
go mem
auto
```

### Measured instruction behaviour

`hw/probe` built with `-DUSERLAND`, run as a static Linux process on RAIDiator 4.1.16.
200,000 random operand pairs per test, identical over 3 runs with the same seed:

| Test | Errors |
|---|---|
| `udiv` | 0 |
| `sdiv` | **4,791**, none with a non-negative dividend |
| `umul` with 2 `nop`s | 0 |
| `umul` directly after a load | 0 |

First failure: `0x8b317081 / 0x40` gives `0xfe2cc5c4`; the correct result is `0xfe2cc5c3`.

This confirms bug 940 on real hardware. The multiply hazard did not reproduce in this
sequence, so it stays unknown; the V7-only policy does not depend on it.

### Measured registers

Read in supervisor mode by `hw/kmod/padre_regs.ko` on RAIDiator 4.1.16. The module is built
with GCC 16.2.0 and loads into the stock 2.6.17.14 kernel without trouble.

| Register | Value | Reading |
|---|---|---|
| PSR | `004000e2` | **impl 0, ver 0**, not a LEON ID (impl 0xf, ver 3); EF and EC clear |
| WIM | `00000008` | |
| TBR | `80004060` | TBA `0x80004000`; last trap 0x06 |
| `SYS_ID` | `31071000` | IT3107; `setup.c` tests bit 12 to tell it from the IT3102 |
| `SYS_CTRL` | `00000000` | |
| `SYS_STRAP` | `00070076` | Meaning unknown |
| `LP_VERSION` | `00000001` | HW 0, FW 1, as in `/proc/cpuinfo` |
| `PCI_FEATURE` | `0000302c` | `PCION` set |
| `MMU_CONTROL` | `10000083` | IE and DE on, NF off; bits 7 and 28 unknown |
| `MMU_CACHEABLE` | `bf000000` | Top 16 MiB of the 1 GiB of RAM non-cached |
| `MMU_PGT` | `7f0fb001` | EN set; table at AHB `0x7f0fb000`, kernel VA `0xbf0fb000` |
| `DCMU_CTRL` | `40332805` | EN, DC3; write-back (WT clear) |
| `ICMU_CTRL` | `40334005` | EN, IC3 |

`MMU_PGT` confirms that kernel VA = AHB address + `0x40000000` for DDR, and that the tables
sit in the non-cached area. The DCMU and ICMU values are AHB addresses of the DC3 and IC3
regions, as `head.S` programs them.

NetBSD cannot identify this CPU from a table of Sun or LEON IDs; `impl 0, ver 0` is what
the port has to expect.

### Bare-metal run

`hw/kmod/padre_go.ko` copied `hw/probe/probe-boot.bin` to `0x90000000` and entered it with
traps off, ending Linux. The image ran from its own trap table, with C code built by
GCC 16.2.0, window overflow and underflow traps served, and output on polled UART0. It
stopped the network as expected, so the machine needs a power cycle afterwards.

- The `sdiv` result matched the Linux run exactly, so it is not a Linux artefact.
- **`flush` is unimplemented:** `flush %g0` took trap 0x02 (illegal instruction) at its own
  PC, reported as `TRAP 90000020 900022a4 900022a8`.
- `boot_param` was still readable: `mem_size 0x40000000`, `cpu_type 0`, `iboot 1.00a043`.
- PSR read back as `004001e7` after writing PIL 15 (`0xfe0`): the PIL field returned **1**.
  Only part of the field may be implemented. To check before relying on PIL levels. [unknown]
- `MMU_CONTROL` read `10000103` here and `10000083` under Linux, so bits 7 and 8 change at
  run time. `MMU_DFSR` was `00000200`, `MMU_IFSR` `00000803`, `MMU_DFADDR` `000b2908`.
- TBR kept the old trap type in its `tt` field while the new base was in place. The value
  `0x90000020` came from the kernel's last trap, the `flush` loop in the loader.

### Console and flash access

- `/etc/inittab` runs `getty -L ttyS1 9600`, and `padre_serial.c` registers the console with
  index 1. The rear console is UART0 (`0x60000060`), confirmed by polled output from bare metal.
- A burst of 6 command lines at 9600 baud stopped the console responding, while the system
  stayed up on the network. Reopening the port on the PC did not help. The cause is
  unconfirmed; `tools/serial-iboot.py` now paces its bytes.
- **NAND and PCI exclude each other.** `padre_hwcontrol` clears `PCI_FEATURE_PCION` while
  NAND is selected. Reading `mtdblock0` killed the EHCI controller (`HC died`) and returned
  I/O errors. `/dev/mtd0` returned 0 bytes. No NAND read has succeeded from Linux, and
  iboot has not been dumped.

`/proc` on the running system:

- `/proc/cpuinfo`: `ASIC: IT3107`, `neon version: 0`, `Softfpu`, `LP HW.FW version: 0.1`.
- `/proc/version`: `2.6.17.14ReadyNAS`, built with `gcc version 3.3.5 (Infrant 3.3.5-1)`.
- `/proc/interrupts`: `serial` on IRQ 8 only, `PADRE_LIST_PROCESSOR` on 44, `eth0` on 41,
  `ide1`/`ide2` on 32/33, timer on 46, `ehci_hcd`/`uhci_hcd` sharing 27, `powsw` on 11.
- `VmallocTotal: 131008 kB`, matching the area below.

LP stands for **List Processor**.

`init_mm.pgd bf0ff000` sits in the top 16 MiB of RAM, matching the non-cached page table
scheme below. The `64 MiB` NAND size is confirmed.

## Conclusion

**The IT3107 is not LEON compatible.** The integer instruction set is SPARC V8 style, and
the headers call the core the "NEON PROCESSOR", but the privileged architecture is
custom. LEON assets (GRLIB drivers, code assuming an SRMMU) cannot be reused.

| Item | Typical LEON / sun4m | IT3107 |
|---|---|---|
| MMU | SRMMU (3 levels, contexts) | Custom (2 levels 10/10/12, no contexts) |
| PTE | SRMMU format | Custom bit layout |
| `flush` instruction | Present | **Absent; the kernel emulates it from the illegal instruction trap** |
| `sdiv` | Correct | **Wrong results for negative dividends** |
| FPU | Depends on configuration | None (`-mno-fpu` plus `math-emu`) |
| Peripherals | APB/AMBA PnP | Custom IP at fixed addresses |
| Second processor | None | **LP (a SPARC core handling disk work)** |

## CPU

- PSR/WIM/TBR and the trap table are standard SPARC. 8 register windows (`sll %wim, 7` in `etrap.S`). [confirmed]
- The upper 16 bits of `SYS_ID` hold the chip number (`0x3107`, `0x1008`). [confirmed]
- Interrupts use PIL 1-15 plus a custom INTC (48 lines, H/L pairs of MASK/ICAUSE/ICLR). [confirmed]
- Atomics in use are `ldstub` and `swap`. There is no `cas`. [confirmed]
- Sources disagree on the clock: 280 MHz in one, ~400 MHz in another. [unknown]

### Multiply/divide errata

| Bug | Description | Workaround |
|---|---|---|
| 940 | `sdiv`/`sdivcc` are wrong when the dividend is negative (`CONFIG_PADRE_DIV_BUG`) | Divide absolute values with `udiv`, then restore the sign |
| Unnamed | Multiply seems to need 2 instructions before it (`dont delete these two nops`) | 2 `nop`s before `umul`/`smul` |

- The glibc patch changes `sparcv8` from `-mcpu=v8` to `-mcpu=v7`. [confirmed]
- A `umul` right after a load is also padded with `nop`s. The exact hazard is not known. [unknown]
- `hw/probe` confirmed the `sdiv` bug; see "Measured instruction behaviour".

**Policy: never use V8 multiply/divide instructions; all code is V7.** Not generating
`smul`/`umul`/`sdiv`/`udiv` avoids both errata. Reliability comes before performance.

Conflict between sources: `Zalaano666/readynas-sparc` reports success with `-mcpu=v8`,
while the DIET and Infrant sources both show the bug. Hardware `sdiv` only shows symptoms
with a negative dividend, so those binaries probably just appear to work. [inferred]

### Other errata

- Bug 505: dummy write to `0x66660000` after INTC mask writes. Active when
  `CONFIG_PADRE_TRAP10_BUG` is undefined, which is the default configuration. [confirmed]
- Bug 893: enabled by default in Kconfig; no code use checked yet.
- Bug 179: disabled by default.
- `CONFIG_PADRE_UNEXPECT_TLBTRAP`: described as "chip can generate unexpected TLB trap".

## Memory map

With the MMU on, kernel virtual address = physical (AHB) address + `0x40000000`.

| Virtual | Physical | Contents |
|---|---|---|
| `0x00000000-0x3fffffff` | Page tables | User space (**1 GiB only**) |
| `0x40000000` | `0x00000000` | ROM space |
| `0x50000000` | `0x10000000` | PCI (`0xcf8/0xcfc` style config) |
| `0x60000000` | `0x20000000` | APB peripherals |
| `0x70000000` | `0x30000000` | S3 on-chip SRAM |
| `0x80000000` | `0x40000000` | DDR (kernel direct map) |
| `0xf8000000-0xffff0000` | Page tables | vmalloc area (just under 128 MiB) |

Whether the direct map above `0x40000000` is fixed in hardware is inferred from `head.S`
using it before page tables exist. The exact MMU behaviour is unconfirmed. [inferred]

### Main registers

| Address | Block |
|---|---|
| `0x60000000` | INTC0 |
| `0x60000040` | Timers |
| `0x60000060` / `0x60000070` | UART0 / UART1 (ttyS1 / ttyS0; **the rear console is ttyS1**) |
| `0x60000080` | SYS (`+0x10` ID, `+0x14` CTRL, `+0x18` STRAP) |
| `0x600000c0` | GPIO |
| `0x60000100` | PCI (also NAND CE) |
| `0x60000200-` | IDE0-7 |
| `0x60001000` | LP (`+0x00c` CH_TRIG, `+0x028` VERSION) |
| `0x60002000` | MC (DMA channels at `+0x800`) |
| `0x60003000` / `0x60004000` | CP0 / CP1 (crypto) |
| `0x60005000` / `0x60006000` | DJ / RXA (XOR) |
| `0x60007000-` | SATA0-7 |
| `0x60010000` / `0x60020000` | GMAC0 / GMAC1 |

UART IRQs are 7 (UART1) and 8 (UART0); timers are 47-45. [confirmed]

## MMU

- Controlled through `ASI 0x10` (`CONTROL`, `DFSR`, `IFSR`, `DFADDR`, `CACHEABLE`, `PGT`). [confirmed]
- Switching spaces writes a physical address to `PGT` with `EN` and `FL` (TLB flush) set. [confirmed]
- 2-level tables with 4 KiB pages. The PGD has 1024 entries of 4 MiB each. [confirmed]
- Linux selects 16 KiB pages, but that is a software scheme of 4 consecutive 4 KiB PTEs;
  the hardware page is 4 KiB. [confirmed]
- Page tables live in non-cached memory (`CONFIG_PADRE_RESERVE_DMA`). [confirmed]
- Traps 0x3c/0x2c are MMU misses. Software only calls `do_sparc_fault`, so TLB refill
  appears to be a hardware walker. [inferred]
- `sta` to `ASI 0x11` invalidates one page of TLB. [confirmed]
- Whether hardware updates the A/D bits is not known. [unknown]

PTE bits: `U=9 R=8 W=7 X=6 C=5 T=4 A=3 D=2 Z=1 V=0`, PPN in `0xfffff000`. [confirmed]

## Caches

- Split I/D, `ASI 0x14`/`0x15`. The ICMU reads as 4 ways of 4 KiB, 16 KiB in total. [inferred]
- 16-byte lines. [confirmed]
- **DMA is not snooped.** Drivers maintain the caches explicitly, e.g. `flush_cache_dma`. [confirmed]
- Physical memory above `MMU_CACHEABLE` is non-cached (top 16 MiB of RAM by default). [confirmed]
- IC3/DC3 let part of the cache act as tightly coupled memory. Linux places fast paths
  there, but the port does not need them early on.

### `flush` emulation

The instruction is not implemented, so the illegal instruction trap decodes `flush`,
computes the effective address, cleans the D-cache and invalidates the matching 4 I-cache
ways (`bad_instruction` in `arch/padre/kernel/entry.S`). [confirmed]
glibc is changed to always run `flush` when rewriting the PLT instead of relying on
`HWCAP_SPARC_FLUSH`.

## LP processor

- A second SPARC core. At boot the main CPU releases it through `SYS_CTRL` and loads the
  arrays from `lpram.c` into its I/D RAM. [confirmed]
- The firmware is `lp_s_code` (10,912 bytes) and `lp_p_code` (4,400 bytes). The kernel's
  `head.S` loads `lp_s_*`. It disassembles with 0 unknown instructions. [confirmed]
- It drives SATA, DJ, MC DMA and S3 SRAM directly at physical addresses. [confirmed]
- Drivers write a command bus address to `0x60001100 + 4*(ch+4)` and then hit `CH_TRIG`
  (`intr_to_lp` in `padre_io.ko`). [confirmed]
- Whether SATA can run without LP, and whether Ethernet needs it, is not known.
  `padre_gmac` has `padre_gmac_init_dj_channel`. [unknown]

## Boot and iboot

- The NAND driver describes a 512 Mibit (64 MiB) part; the "64 GiB" in the reference
  README looks like a typo. The first 1 MiB holds the plaintext iboot. [confirmed]
- The kernel and initrd in the rest are encrypted; iboot decrypts them with the HW engine. [confirmed]
- The kernel runs from `0x80004000`, and its start doubles as the trap table. [confirmed]
- iboot leaves `boot_param` (`mem_size`, MAC, VPD, 512 bytes) at `0x80000000`. [confirmed]
- The initrd is fixed at physical `0x01000000-0x02000000`. [confirmed]
- Serial is 3.3 V, 9600 8N1. Pressing `i` during boot opens the iboot console (`tftp`,
  `go`). This was reported on the NV, not confirmed on the Duo.
- **Binaries loaded over tftp must also be encrypted.** The `rn_nops` workaround only
  works on iboot 1.00a030. Also reported on the NV.

## Drivers

Binary only: `padre_io` (SATA/X-RAID), `padre_gmac`, `padre_i2c`, `padre_p0_led_button`,
`padre_des`. **All modules keep their DWARF and symbols.**

| Module | Functions | Notes |
|---|---|---|
| `padre_io.ko` | 196 | `.text` 141 KiB |
| `padre_gmac.ko` | 77 | `.text` 68 KiB, `depends=padre_i2c` |

Source available: `padre_serial.c`, `padre_nand_flash.c`, `padre_nor_flash.c`,
`padre_i2c_rtc.c`, `pcic.c` (arch/padre).

## Licensing notes

Linux `arch/padre` is GPL. Code in the NetBSD tree is written from hardware facts, not
copied. The firmware in `lpram.c` ships in a GPL distribution, but stays a separate file
until its redistribution terms are checked. Infrant's modules are proprietary, so
analysis results are recorded as specifications and no code is derived from them.
