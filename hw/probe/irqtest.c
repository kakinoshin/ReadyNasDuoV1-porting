/*
 * IT3107 interrupt measurements, run as a self-contained image from boot.S.
 *
 *  - which PSR.PIL bits exist and which of them block interrupts
 *  - the timer rate, against the UART's character time
 *  - the trap level taken by interrupts forced through IFORCE
 *
 * Register offsets are from include/asm-padre/padre0.h.
 */
typedef unsigned int u32;

#define REG(a)		(*(volatile u32 *)(a))

#define UART_DATA	REG(0x60000060)
#define UART_STAT	REG(0x60000064)
#define UART_STAT_TH	0x4

#define INTC_MASK_H	REG(0x60000000)
#define INTC_MASK_L	REG(0x60000004)
#define INTC_FORCE_H	REG(0x60000010)
#define INTC_FORCE_L	REG(0x60000014)
#define INTC_CLR_H	REG(0x60000018)
#define INTC_CLR_L	REG(0x6000001c)

#define TM_CTRL		REG(0x60000040)
#define TM_PS		REG(0x60000044)
#define TM_TIMER1	REG(0x60000054)
#define TM_CTRL_STP	0x00001000

/* Timer 1 with the values Linux uses: 100 Hz if its constants are right. */
#define TM_PRESCALE	24
#define TM_COUNT	499
#define TM_ENABLE	(0x20 | 0x200 | 0x2 | 0x2000)	/* AUTO, LOAD, INTEN, SEL */
#define IRQ_TIMER1_H	(1u << 14)

/* Writing this address follows every INTC mask change in Linux (bug 505). */
#define BUG505		REG(0x66660000)

#define PSR_PIL_SHIFT	8
#define PSR_PIL_MASK	0xf00

u32 rd_psr(void);
void wr_psr(u32);

/* Shared with irq_handler in irqtrap.S. */
volatile u32 irq_count, irq_last_tt, irq_cause_h, irq_cause_l, irq_rearm_h;

static void
putc(int c)
{
	while ((UART_STAT & UART_STAT_TH) == 0)
		continue;
	UART_DATA = c;
}

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

static void
mask_set(u32 h, u32 l)
{
	INTC_MASK_H = h;
	INTC_MASK_L = l;
	BUG505 = 0;
}

/* Sends n characters and so takes n * 10 bit times at 9600 baud. */
static void
wait_chars(u32 n)
{
	while (n-- != 0)
		putc('.');
	puts("\n");
}

static void
set_pil(u32 pil)
{
	wr_psr((rd_psr() & ~PSR_PIL_MASK) | (pil << PSR_PIL_SHIFT));
}

static void
test_pil_bits(void)
{
	u32 pil;

	puts("PIL write -> read\n");
	for (pil = 0; pil < 16; pil++) {
		set_pil(pil);
		puthex(pil);
		puts(" -> ");
		puthex((rd_psr() & PSR_PIL_MASK) >> PSR_PIL_SHIFT);
		puts("\n");
	}
}

static void
test_timer(void)
{
	static const u32 pils[] = { 0, 1, 2, 4, 8, 15 };
	u32 i, start;

	puts("timer: 960 chars, about 1 s\n");
	TM_CTRL = TM_CTRL_STP;
	TM_PS = TM_PRESCALE;
	TM_TIMER1 = TM_COUNT;
	irq_rearm_h = IRQ_TIMER1_H;
	mask_set(IRQ_TIMER1_H, 0);
	TM_CTRL = TM_ENABLE;

	set_pil(0);
	start = irq_count;
	wait_chars(960);
	puts("ticks ");
	puthex(irq_count - start);
	puts(" tt ");
	puthex(irq_last_tt);
	puts(" cause_h ");
	puthex(irq_cause_h);
	puts("\n");

	puts("ticks over 96 chars by PIL\n");
	for (i = 0; i < sizeof(pils) / sizeof(pils[0]); i++) {
		set_pil(pils[i]);
		start = irq_count;
		wait_chars(96);
		start = irq_count - start;
		set_pil(0);
		puthex(pils[i]);
		puts(": ");
		puthex(start);
		puts("\n");
	}

	irq_rearm_h = 0;
	TM_CTRL = TM_CTRL_STP;
	mask_set(0, 0);
	wait_chars(8);
}

static void
force_one(int high, u32 bit)
{
	u32 m = 1u << bit, start = irq_count;
	int i;

	irq_last_tt = 0;
	irq_cause_h = irq_cause_l = 0;
	mask_set(high ? m : 0, high ? 0 : m);
	if (high)
		INTC_FORCE_H = m;
	else
		INTC_FORCE_L = m;
	for (i = 0; i < 100000 && irq_count == start; i++)
		continue;

	puts(high ? "H" : "L");
	puthex(bit);
	puts(" count ");
	puthex(irq_count - start);
	puts(" tt ");
	puthex(irq_last_tt);
	puts(" cause ");
	puthex(irq_cause_h);
	puts(" ");
	puthex(irq_cause_l);
	puts("\n");

	/* In case nothing was delivered. */
	INTC_FORCE_H = 0;
	INTC_FORCE_L = 0;
	mask_set(0, 0);
	INTC_CLR_H = 0xffffffff;
	INTC_CLR_L = 0xffffffff;
}

static void
test_force(void)
{
	u32 bit;

	puts("forced interrupts\n");
	for (bit = 0; bit < 32; bit++)
		force_one(0, bit);
	for (bit = 0; bit < 16; bit++)
		force_one(1, bit);
}

void
probe_main(void)
{
	puts("\nIT3107 irq test\n");
	mask_set(0, 0);
	INTC_FORCE_H = 0;
	INTC_FORCE_L = 0;
	INTC_CLR_H = 0xffffffff;
	INTC_CLR_L = 0xffffffff;

	test_pil_bits();
	set_pil(0);
	test_timer();
	test_force();

	set_pil(15);
	puts("irq test done\n");
}
