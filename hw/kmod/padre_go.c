/*
 * Hand the CPU to a self-contained image from a RAIDiator kernel module.
 *
 * The image is embedded with .incbin, copied to its link address in the kernel
 * direct map, and entered with traps off. Linux does not come back, so the
 * machine needs a power cycle afterwards. If the copy cannot be verified the
 * module fails normally and Linux keeps running.
 *
 * Unused high RAM is overwritten without asking Linux for it. That is fine for
 * a machine that is about to stop, and nothing else runs once PIL is raised.
 *
 * See padre_regs.c for how the module is built without kernel headers.
 */
typedef unsigned int u32;

#define MODULE_SIZE	0x160
#define MODULE_NAME	0x00c
#define MODULE_INIT	0x0c4
#define MODULE_EXIT	0x12c

#define EIO		5

#define IMAGE_BASE	0x90000000
#define CACHE_LINE	16
#define PSR_PIL		0xf00
#define PSR_ET		0x020

int printk(const char *, ...);

__asm__(
"	.section .rodata\n"
"	.align	4\n"
"	.globl	image_start\n"
"image_start:\n"
"	.incbin	\"" IMAGE_FILE "\"\n"
"	.globl	image_end\n"
"image_end:\n"
"	.previous\n");
extern const char image_start[], image_end[];

static inline u32
read_psr(void)
{
	u32 v;

	__asm__ __volatile__("rd %%psr, %0\n\tnop\n\tnop\n\tnop" : "=r"(v));
	return v;
}

static inline void
write_psr(u32 v)
{
	__asm__ __volatile__("wr %0, 0, %%psr\n\tnop\n\tnop\n\tnop" : : "r"(v));
}

int
init_module(void)
{
	volatile char *dst = (volatile char *)IMAGE_BASE;
	u32 size = image_end - image_start;
	u32 psr, i;

	/* KERN_EMERG reaches the serial console, so each stage shows there. */
	printk("<0>padre_go: image %u bytes to %08x\n", size, IMAGE_BASE);

	psr = read_psr();
	write_psr(psr | PSR_PIL);

	for (i = 0; i < size; i++)
		dst[i] = image_start[i];
	for (i = 0; i < size; i++) {
		if (dst[i] != image_start[i]) {
			write_psr(psr);
			printk("<0>padre_go: verify failed at %u\n", i);
			return -EIO;
		}
	}

	printk("<0>padre_go: copy verified, entering\n");

	/* The kernel's emulation cleans the D-cache line and drops the I-cache line. */
	for (i = 0; i < size + CACHE_LINE; i += CACHE_LINE)
		__asm__ __volatile__("flush %0" : : "r"(IMAGE_BASE + i));

	write_psr((psr | PSR_PIL) & ~PSR_ET);
	__asm__ __volatile__("jmp %0\n\tnop" : : "r"(IMAGE_BASE));

	for (;;)
		continue;
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
	.name = "padre_go",
	.init = init_module,
	.exit = cleanup_module,
};

static const char modinfo[]
    __attribute__((section(".modinfo"), used)) =
	"vermagic=2.6.17.14ReadyNAS mod_unload gcc-3.3\0"
	"license=GPL";
