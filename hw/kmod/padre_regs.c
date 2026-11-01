/*
 * Read-only register dump for the IT3107, as a RAIDiator kernel module.
 *
 * Built with a modern compiler without the kernel headers, so the fields of
 * struct module that the 2.6.17.14ReadyNAS loader needs are placed by offset.
 * The offsets come from a stock module of that kernel; the loader rejects the
 * module if the section size is wrong.
 *
 * init_module() always fails, so nothing stays loaded. Each register name is
 * logged before its read, so a read that traps can be identified.
 */
typedef unsigned int u32;

#define MODULE_SIZE	0x160
#define MODULE_NAME	0x00c
#define MODULE_INIT	0x0c4
#define MODULE_EXIT	0x12c

#define ENODEV		19


#define MMU_CONTROL	0x04
#define MMU_CACHEABLE	0x1c
#define MMU_PGT		0x24
#define CMU_CTRL	0x00030000

#define MMIO(a)		(*(volatile u32 *)(a))
#define LDA(asi, a) ({							\
	u32 _v;								\
	__asm__ __volatile__("lda [%1] " #asi ", %0"			\
	    : "=r"(_v) : "r"(a));					\
	_v;								\
})
#define RDSR(reg) ({							\
	u32 _v;								\
	__asm__ __volatile__("rd %%" #reg ", %0" : "=r"(_v));		\
	_v;								\
})

int printk(const char *, ...);

#define SHOW(name, expr) do {						\
	u32 _v;								\
	printk("<6>padre_regs: %s\n", name);				\
	_v = (expr);							\
	printk("<6>padre_regs: %s = %08x\n", name, _v);			\
} while (0)

#define asi_mmu(a)	LDA(0x10, a)
#define asi_icmu(a)	LDA(0x14, a)
#define asi_dcmu(a)	LDA(0x15, a)

int
init_module(void)
{
	SHOW("psr", RDSR(psr));
	SHOW("wim", RDSR(wim));
	SHOW("tbr", RDSR(tbr));
	SHOW("sys_id", MMIO(0x60000090));
	SHOW("sys_ctrl", MMIO(0x60000094));
	SHOW("sys_strap", MMIO(0x60000098));
	SHOW("lp_version", MMIO(0x60001028));
	SHOW("pci_feature", MMIO(0x6000010c));
	SHOW("mmu_control", asi_mmu(MMU_CONTROL));
	SHOW("mmu_cacheable", asi_mmu(MMU_CACHEABLE));
	SHOW("mmu_pgt", asi_mmu(MMU_PGT));
	SHOW("dcmu_ctrl", asi_dcmu(CMU_CTRL));
	SHOW("icmu_ctrl", asi_icmu(CMU_CTRL));
	return -ENODEV;
}

void
cleanup_module(void)
{
}

struct this_module {
	char pad0[MODULE_NAME];
	char name[MODULE_INIT - MODULE_NAME];
	int (*init)(void);
	char pad1[MODULE_EXIT - MODULE_INIT - 4];
	void (*exit)(void);
	char pad2[MODULE_SIZE - MODULE_EXIT - 4];
};

struct this_module __this_module
    __attribute__((section(".gnu.linkonce.this_module"), aligned(32))) = {
	.name = "padre_regs",
	.init = init_module,
	.exit = cleanup_module,
};

static const char modinfo[]
    __attribute__((section(".modinfo"), used)) =
	"vermagic=2.6.17.14ReadyNAS mod_unload gcc-3.3\0"
	"license=GPL";
