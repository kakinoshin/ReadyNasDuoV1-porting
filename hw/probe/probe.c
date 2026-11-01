/*
 * IT3107 bare-metal probe.
 *
 * Reports chip identity and control register state on the console UART,
 * then checks the V8 multiply/divide instructions against V7 software
 * references. Register reads only, apart from the UART: no trap, MMU or
 * cache state is changed.
 *
 * With -DUSERLAND it runs as a static Linux process on RAIDiator instead:
 * only the instruction tests run, and output goes to stdout.
 *
 * Addresses come from include/asm-padre/padre0.h in the NETGEAR GPL source.
 */
typedef unsigned int u32;
typedef int s32;

/* The rear console is ttyS1, which padre_serial.c maps to UART0. */
#define UART_DATA	(*(volatile u32 *)0x60000060)
#define UART_STAT	(*(volatile u32 *)0x60000064)
#define UART_STAT_TH	0x4

#define SYS_ID		(*(volatile u32 *)0x60000090)
#define SYS_CTRL	(*(volatile u32 *)0x60000094)
#define SYS_STRAP	(*(volatile u32 *)0x60000098)
#define LP_VERSION	(*(volatile u32 *)0x60001028)

#define MMU_CONTROL	0x04
#define MMU_DFSR	0x10
#define MMU_IFSR	0x14
#define MMU_DFADDR	0x18
#define MMU_CACHEABLE	0x1c
#define MMU_PGT		0x24
#define CMU_CTRL	0x00030000

/* boot_param from iboot, include/asm-padre/system.h */
#define BOOT_MEMSIZE	(*(volatile u32 *)0x80000000)
#define BOOT_CPUTYPE	(*(volatile u32 *)0x80000004)
#define BOOT_VERSION	((volatile char *)0x80000010)

s32 hw_sdiv(s32, s32);
u32 hw_udiv(u32, u32);
u32 hw_umul_after_load(u32 *, u32 *, u32 *);
u32 hw_umul_padded(u32 *, u32 *, u32 *);
#ifndef USERLAND
u32 rd_psr(void), rd_tbr(void), rd_wim(void);
u32 rd_mmu(u32), rd_icmu(u32), rd_dcmu(u32);
#endif

#define ITERATIONS	200000

#ifdef USERLAND
void sys_write(int, const char *, u32);

static char outbuf[256];
static u32 outlen;

static void
putc(int c)
{
	outbuf[outlen++] = c;
	if (c == '\n' || outlen == sizeof(outbuf)) {
		sys_write(1, outbuf, outlen);
		outlen = 0;
	}
}
#else
static void
putc(int c)
{
	while ((UART_STAT & UART_STAT_TH) == 0)
		continue;
	UART_DATA = c;
}
#endif

static void
puts(const char *s)
{
	for (; *s != 0; s++) {
		if (*s == '\n')
			putc('\r');
		putc(*s);
	}
}

static void
puthex(u32 v)
{
	int i;

	for (i = 28; i >= 0; i -= 4)
		putc("0123456789abcdef"[(v >> i) & 0xf]);
}

#ifndef USERLAND
static void
kv(const char *k, u32 v)
{
	puts(k);
	puts(" = 0x");
	puthex(v);
	puts("\n");
}

#endif /* !USERLAND */

static u32 rng = 2463534242u;

static u32
xorshift(void)
{
	rng ^= rng << 13;
	rng ^= rng >> 17;
	rng ^= rng << 5;
	return rng;
}

/* Shift-subtract; compiled for V7, so no hardware divide involved. */
static u32
sw_udiv(u32 n, u32 d)
{
	u32 q = 0, r = 0;
	int i;

	for (i = 31; i >= 0; i--) {
		r = (r << 1) | ((n >> i) & 1);
		if (r >= d) {
			r -= d;
			q |= 1u << i;
		}
	}
	return q;
}

static s32
sw_sdiv(s32 a, s32 b)
{
	u32 ua = a < 0 ? 0u - (u32)a : (u32)a;
	u32 ub = b < 0 ? 0u - (u32)b : (u32)b;
	u32 q = sw_udiv(ua, ub);

	return (a < 0) != (b < 0) ? (s32)(0u - q) : (s32)q;
}

static void
sw_umul(u32 a, u32 b, u32 *hi, u32 *lo)
{
	u32 h = 0, l = 0;
	int i;

	for (i = 0; i < 32; i++) {
		if (b & (1u << i)) {
			u32 add = i == 0 ? 0 : a >> (32 - i);
			u32 low = a << i;

			l += low;
			h += add + (l < low);
		}
	}
	*hi = h;
	*lo = l;
}

static void
report(const char *name, u32 errs, u32 a, u32 b, u32 got, u32 want)
{
	puts(name);
	puts(": errors 0x");
	puthex(errs);
	puts(" of 0x");
	puthex(ITERATIONS);
	if (errs != 0) {
		puts("  first ");
		puthex(a);
		puts(" ");
		puthex(b);
		puts(" hw ");
		puthex(got);
		puts(" sw ");
		puthex(want);
	}
	puts("\n");
}

static void
kvnz(const char *k, u32 v)
{
	puts(k);
	puts(": 0x");
	puthex(v);
	puts("\n");
}

static void
test_sdiv(void)
{
	u32 errs = 0, nonneg = 0, fa = 0, fb = 0, fg = 0, fw = 0;
	int i;

	for (i = 0; i < ITERATIONS; i++) {
		s32 a = (s32)xorshift(), b;
		s32 g, w;

		do {
			b = (s32)xorshift() >> (xorshift() & 31);
		} while (b == 0 || (a == (s32)0x80000000 && b == -1));
		g = hw_sdiv(a, b);
		w = sw_sdiv(a, b);
		if (g != w) {
			if (a >= 0)
				nonneg++;
			if (errs++ == 0) {
				fa = a; fb = b; fg = g; fw = w;
			}
		}
	}
	report("sdiv", errs, fa, fb, fg, fw);
	kvnz("sdiv errors with dividend >= 0", nonneg);
}

static void
test_udiv(void)
{
	u32 errs = 0, fa = 0, fb = 0, fg = 0, fw = 0;
	int i;

	for (i = 0; i < ITERATIONS; i++) {
		u32 a = xorshift(), b;
		u32 g, w;

		do {
			b = xorshift() >> (xorshift() & 31);
		} while (b == 0);
		g = hw_udiv(a, b);
		w = sw_udiv(a, b);
		if (g != w && errs++ == 0) {
			fa = a; fb = b; fg = g; fw = w;
		}
	}
	report("udiv", errs, fa, fb, fg, fw);
}

static void
test_umul(const char *name, u32 (*mul)(u32 *, u32 *, u32 *))
{
	u32 errs = 0, fa = 0, fb = 0, fg = 0, fw = 0;
	int i;

	for (i = 0; i < ITERATIONS; i++) {
		u32 a = xorshift(), b = xorshift();
		u32 gh, gl, wh, wl;

		gl = mul(&a, &b, &gh);
		sw_umul(a, b, &wh, &wl);
		if ((gh != wh || gl != wl) && errs++ == 0) {
			fa = a; fb = b; fg = gl; fw = wl;
		}
	}
	report(name, errs, fa, fb, fg, fw);
}

#ifndef USERLAND
static void
report_identity(void)
{
	volatile char *v;
	int i;

	kv("SYS_ID       ", SYS_ID);
	kv("SYS_CTRL     ", SYS_CTRL);
	kv("SYS_STRAP    ", SYS_STRAP);
	kv("LP_VERSION   ", LP_VERSION);
	kv("PSR          ", rd_psr());
	kv("TBR          ", rd_tbr());
	kv("WIM          ", rd_wim());
	kv("MMU_CONTROL  ", rd_mmu(MMU_CONTROL));
	kv("MMU_DFSR     ", rd_mmu(MMU_DFSR));
	kv("MMU_IFSR     ", rd_mmu(MMU_IFSR));
	kv("MMU_DFADDR   ", rd_mmu(MMU_DFADDR));
	kv("MMU_CACHEABLE", rd_mmu(MMU_CACHEABLE));
	kv("MMU_PGT      ", rd_mmu(MMU_PGT));
	kv("ICMU_CTRL    ", rd_icmu(CMU_CTRL));
	kv("DCMU_CTRL    ", rd_dcmu(CMU_CTRL));
	kv("boot mem_size", BOOT_MEMSIZE);
	kv("boot cpu_type", BOOT_CPUTYPE);
	puts("boot version : ");
	for (v = BOOT_VERSION, i = 0; i < 64 && *v != 0; v++, i++)
		putc(*v);
	puts("\n");
}
#endif

void
probe_main(void)
{
	puts("\nIT3107 probe\n");
#ifndef USERLAND
	report_identity();
#endif
	test_udiv();
	test_sdiv();
	test_umul("umul padded    ", hw_umul_padded);
	test_umul("umul after load", hw_umul_after_load);
	puts("probe done\n");
#ifdef PROBE_TRAP
	/* Expected to end in the trap report from boot.S. */
	puts("executing flush\n");
	__asm__ __volatile__("flush %g0");
	puts("flush did not trap\n");
#endif
}
