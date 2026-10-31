# Infrant IT3107 (Padre / NEON) hardware analysis

Primary sources, from the NETGEAR GPL archive `RNR4_RND4_RND2_4.1.6_WW_src.zip`:
`linux-2.6.17.8/arch/padre`, `include/asm-padre`, `kernel_modules/` and
`glibc-2.3.2.ds1/debian/patches/infrant.dpatch`.
The official URL returns 404, so the archive comes from the Wayback Machine. The parts
used here are in `refs/gpl/extracted/`.

Confidence labels:
**[confirmed]** stated in source / **[inferred]** reasoned from source, not checked on
hardware / **[unknown]** not yet determined.
Nothing here has been checked on real hardware yet.

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
- `hw/probe` measures both.

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
| `0x60000060` / `0x60000070` | UART0 / UART1 (**the console is UART1**) |
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
