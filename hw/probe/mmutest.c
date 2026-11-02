/*
 * IT3107 MMU measurements, run as a self-contained image from boot.S.
 *
 * Installs its own tables in the non-cached top of RAM and accesses test pages
 * from supervisor mode. Each line gives the fault count, trap type, DFSR, IFSR,
 * fault address, the value read and the PTE afterwards. The line starts before
 * the access, so a reset identifies the access that caused it.
 *
 *  - whether a TLB miss is refilled from the tables by hardware
 *  - whether hardware sets A and D, or faults without them
 *  - how permission and invalid faults are reported
 *  - whether the TLB keeps stale and invalid entries, and how to remove them
 */
typedef unsigned int u32;

#define REG(a)		(*(volatile u32 *)(a))

#define UART_DATA	REG(0x60000060)
#define UART_STAT	REG(0x60000064)
#define UART_STAT_TH	0x4

/* Physical address + 0x40000000 is the direct map; this part is non-cached. */
#define VA(pa)		((pa) + 0x40000000)
#define PGD_PA		0x7f000000
#define PT_USER_PA	0x7f001000
#define PT_KVA_PA	0x7f002000
#define PAGE_PA(i)	(0x7f010000 + (i) * 0x1000)

#define USER_VA(i)	(0x00400000 + (i) * 0x1000)
#define KVA_VA		0xf8000000
#define NOPDE_VA	0x00800000

#define MMU_PGT		0x24
#define PGT_EN		0x1
#define PGT_FL		0x2

#define U		0x200
#define R		0x100
#define W		0x080
#define X		0x040
#define C		0x020
#define A		0x008
#define D		0x004
#define V		0x001
#define PDE		(U | W | D | R | X | C | V)

/* retl; mov 1, %o0 */
#define INSN_RETL	0x81c3e008
#define INSN_MOV1	0x90102001

u32 rd_mmu(u32);
void wr_mmu(u32, u32);
void wr_tlb(u32, u32);
u32 probe_read(u32);
void probe_write(u32, u32);

/* Shared with mmu_fault in mmutrap.S. */
volatile u32 fault_count, fault_tt, fault_dfsr, fault_ifsr, fault_addr;

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
	putc(' ');
}

static void
begin(const char *name)
{
	puts(name);
	puts(": ");
	fault_count = fault_tt = fault_dfsr = fault_ifsr = fault_addr = 0;
}

static void
report(u32 val, u32 pte)
{
	putc('0' + fault_count);
	putc(' ');
	puthex(fault_tt);
	puthex(fault_dfsr);
	puthex(fault_ifsr);
	puthex(fault_addr);
	puthex(val);
	puthex(pte);
	puts("\n");
}

#define UPTE(i)		REG(VA(PT_USER_PA) + (i) * 4)

/* Page i, mapping page i of the test memory, which holds a recognisable word. */
static void
map(int i, u32 bits)
{
	REG(VA(PAGE_PA(i))) = 0x11110000 + i;
	UPTE(i) = bits != 0 ? PAGE_PA(i) | bits : 0;
}

static void
t_read(const char *name, int i, u32 bits)
{
	u32 v;

	map(i, bits);
	begin(name);
	v = probe_read(USER_VA(i));
	report(v, UPTE(i));
}

static void
t_write(const char *name, int i, u32 bits)
{
	map(i, bits);
	begin(name);
	probe_write(USER_VA(i), 0x22220000 + i);
	report(REG(VA(PAGE_PA(i))), UPTE(i));
}

static void
t_fetch(const char *name, int i, u32 bits)
{
	u32 v;

	REG(VA(PAGE_PA(i))) = INSN_RETL;
	REG(VA(PAGE_PA(i)) + 4) = INSN_MOV1;
	UPTE(i) = PAGE_PA(i) | bits;
	begin(name);
	v = ((u32 (*)(void))USER_VA(i))();
	report(v, UPTE(i));
}

static void
again(const char *name, int i)
{
	u32 v;

	begin(name);
	v = probe_read(USER_VA(i));
	report(v, UPTE(i));
}

void
probe_main(void)
{
	const u32 rw = U | R | W | C | V;
	u32 v;
	int i;

	puts("\nmmutest: faults tt dfsr ifsr addr value pte\n");
	for (i = 0; i < 1024; i++) {
		REG(VA(PGD_PA) + i * 4) = 0;
		REG(VA(PT_USER_PA) + i * 4) = 0;
		REG(VA(PT_KVA_PA) + i * 4) = 0;
	}
	REG(VA(PGD_PA) + (USER_VA(0) >> 22) * 4) = PT_USER_PA | PDE;
	REG(VA(PGD_PA) + (KVA_VA >> 22) * 4) = PT_KVA_PA | PDE;
	wr_mmu(MMU_PGT, PGD_PA | PGT_EN | PGT_FL);
	puts("tables on\n");

	/* Refill and A/D. */
	t_read("read A D", 0, rw | A | D);
	t_read("read clean old", 1, rw);
	t_write("write clean old", 2, rw);
	t_write("write old", 3, rw | D);
	t_write("write clean", 4, rw | A);
	t_write("write A D", 5, rw | A | D);

	/* Permissions. */
	t_write("write no W", 6, (rw & ~W) | A | D);
	t_read("read no R", 7, (rw & ~R) | A | D);
	t_read("read no U", 8, (rw & ~U) | A | D);
	t_write("write no U", 9, (rw & ~U) | A | D);
	t_read("read invalid", 10, 0);
	t_read("read no V", 11, (rw & ~V) | A | D);

	begin("read no PDE");
	v = probe_read(NOPDE_VA);
	report(v, 0);

	/* Fetch. */
	t_fetch("fetch X", 12, U | R | X | A | D | V);
	t_fetch("fetch clean old", 13, U | R | X | V);
	t_fetch("fetch no X", 14, U | R | A | D | V);
	t_fetch("fetch invalid", 15, 0);

	/* A PTE in the kernel window, with no TLB store. */
	REG(VA(PAGE_PA(16))) = 0x11110016;
	REG(VA(PT_KVA_PA)) = PAGE_PA(16) | R | W | C | A | D | V;
	begin("kva read");
	v = probe_read(KVA_VA);
	report(v, REG(VA(PT_KVA_PA)));

	/* Is a miss remembered once the PTE becomes valid? */
	t_read("neg first", 17, 0);
	UPTE(17) = PAGE_PA(17) | rw | A | D;
	again("neg PTE set", 17);
	wr_tlb(USER_VA(17), 0);
	again("neg after zero", 17);
	wr_tlb(USER_VA(17), UPTE(17));
	again("neg after load", 17);

	/* Is a changed PTE seen without help, and what removes the old one? */
	t_read("stale first", 18, rw | A | D);
	map(19, rw | A | D);
	UPTE(18) = PAGE_PA(19) | rw | A | D;
	again("stale changed", 18);
	wr_tlb(USER_VA(18), 0);
	again("stale after zero", 18);
	wr_mmu(MMU_PGT, PGD_PA | PGT_EN | PGT_FL);
	again("stale after FL", 18);

	/* The same after a removed PTE. */
	t_read("gone first", 20, rw | A | D);
	UPTE(20) = 0;
	again("gone cleared", 20);
	wr_tlb(USER_VA(20), 0);
	again("gone after zero", 20);

	/* Revoking W without a TLB operation. */
	t_write("wp first", 21, rw | A | D);
	UPTE(21) = PAGE_PA(21) | ((rw & ~W) | A | D);
	begin("wp revoked");
	probe_write(USER_VA(21), 0x33330000);
	report(REG(VA(PAGE_PA(21))), UPTE(21));

	/* Second pass: separate the effects found above. */
	t_read("fl first", 22, rw | A | D);
	map(23, rw | A | D);
	UPTE(22) = PAGE_PA(23) | rw | A | D;
	wr_mmu(MMU_PGT, PGD_PA | PGT_EN | PGT_FL);
	again("fl only", 22);

	t_read("store first", 24, rw | A | D);
	map(25, rw | A | D);
	wr_tlb(USER_VA(24), PAGE_PA(25) | rw | A | D);
	again("store new PTE", 24);

	t_read("zero first", 26, rw | A | D);
	map(27, rw | A | D);
	wr_tlb(USER_VA(26), 0);
	UPTE(26) = PAGE_PA(27) | rw | A | D;
	again("zero then PTE", 26);

	/* D and A once an entry is in the TLB. */
	t_read("clean read", 28, rw);
	begin("clean write");
	probe_write(USER_VA(28), 0x22220028);
	report(probe_read(USER_VA(28)), UPTE(28));

	t_read("A hit first", 29, rw | A | D);
	UPTE(29) = PAGE_PA(29) | rw | D;
	again("A hit cleared", 29);

	t_write("D hit first", 30, rw | A | D);
	UPTE(30) = PAGE_PA(30) | rw | A;
	begin("D hit cleared");
	probe_write(USER_VA(30), 0x22220030);
	report(probe_read(USER_VA(30)), UPTE(30));

	t_write("write invalid", 31, 0);

	t_write("wp2 first", 32, rw | A | D);
	wr_tlb(USER_VA(32), 0);
	UPTE(32) = PAGE_PA(32) | ((rw & ~W) | A | D);
	begin("wp2 revoked");
	probe_write(USER_VA(32), 0x33330000);
	report(probe_read(USER_VA(32)), UPTE(32));

	/* The instruction side, with pages that return different values. */
	t_fetch("istale first", 33, U | R | X | A | D | V);
	REG(VA(PAGE_PA(34))) = INSN_RETL;
	REG(VA(PAGE_PA(34)) + 4) = 0x90102002;	/* mov 2, %o0 */
	UPTE(33) = PAGE_PA(34) | U | R | X | A | D | V;
	begin("istale changed");
	v = ((u32 (*)(void))USER_VA(33))();
	report(v, UPTE(33));
	v = UPTE(33);
	wr_tlb(USER_VA(33), 0);
	UPTE(33) = v;
	begin("istale zero PTE");
	v = ((u32 (*)(void))USER_VA(33))();
	report(v, UPTE(33));

	t_fetch("ifl first", 35, U | R | X | A | D | V);
	UPTE(35) = PAGE_PA(34) | U | R | X | A | D | V;
	wr_mmu(MMU_PGT, PGD_PA | PGT_EN | PGT_FL);
	begin("ifl only");
	v = ((u32 (*)(void))USER_VA(35))();
	report(v, UPTE(35));

	puts("mmutest done\n");
	for (;;)
		continue;
}
