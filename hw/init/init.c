/*
 * First user program for NetBSD/padre, without libc. Each line exercises a part
 * of the port that nothing in the kernel can reach by itself.
 */
typedef unsigned int u32;

void	sys_exit(int);
int	sys_write(int, const void *, u32);
int	sys_open(const char *, int);
int	sys_getpid(void);
int	sys_wait4(int, int *, int, void *);
void	*sys_mmap(void *, u32, int, int, int, long, long long);
int	sys_fork(void);
void	do_flush(void *);
int	sys_execve(const char *, char *const *, char *const *);
int	sys_setrlimit(int, const void *);

#define	O_RDWR		2
#define	PROT_RWX	7
#define	MAP_PRIVATE	0x0002
#define	MAP_ANON	0x1000

#define	RLIMIT_CORE	4
#define	SIGSEGV		11

static int bss[4096];

static char shell[] = "/rescue/sh";
static char *shell_argv[] = { shell, "-i", 0 };
static char *shell_envp[] = { "PATH=/rescue", "TERM=vt100", "HOME=/", 0 };

static u32
slen(const char *s)
{
	u32 n = 0;

	while (s[n] != 0)
		n++;
	return n;
}

static void
say(const char *s)
{

	sys_write(1, s, slen(s));
}

static void
check(const char *name, int ok)
{

	say(name);
	say(ok ? ": ok\n" : ": FAILED\n");
}

/*
 * Deep enough to spill every window to the user stack and bring it back. The
 * volatile keeps a frame at each level: gcc turns plain recursion into a loop.
 */
static int __attribute__((noinline))
depth(int n, int a, int b)
{
	volatile int keep = n;
	int r;

	if (n == 0)
		return a + b;
	r = depth(n - 1, b, a + 1) + 1;
	return r + keep - n;
}

/* Runs in a child, which must die on the access and not get past it. */
static void
refused(const char *name, u32 addr, int kind)
{
	volatile u32 *p = (volatile u32 *)addr;
	char digit[4] = { 0, 0, '\n', 0 };
	int pid, rv, status = 0;

	pid = sys_fork();
	if (pid == 0) {
		if (kind == 0)
			(void)*p;
		else if (kind == 1)
			*p = 0;
		else
			((void (*)(void))addr)();
		sys_exit(0);
	}
	rv = sys_wait4(pid, &status, 0, 0);
	if (rv != pid) {
		say("wait4 returned early\n");
		sys_wait4(pid, &status, 0, 0);
	}
	check(name, (status & 0x7f) == SIGSEGV);
	if ((status & 0x7f) != SIGSEGV) {
		say("  status low byte is ");
		digit[0] = '0' + ((status >> 4) & 0xf);
		digit[1] = '0' + (status & 0xf);
		say(digit);
	}
}

int
main(void)
{
	static const unsigned long long nocore[2];
	u32 *code;
	int i, pid, status = -1, sum = 0;

	sys_open("/dev/console", O_RDWR);
	sys_open("/dev/console", O_RDWR);
	sys_open("/dev/console", O_RDWR);
	say("init: running in user mode\n");

	check("getpid", sys_getpid() == 1);
	check("windows", depth(64, 1, 2) == 64 + 1 + 2 + 64);

	for (i = 0; i < 4096; i++)
		bss[i] = i;
	for (i = 0; i < 4096; i++)
		sum += bss[i];
	check("bss", sum == 4095 * 4096 / 2);

	code = sys_mmap(0, 4096, PROT_RWX, MAP_ANON | MAP_PRIVATE, -1, 0, 0);
	check("mmap", code != (void *)-1);
	code[0] = 0x81c3e008;		/* retl */
	code[1] = 0x9010202a;		/* mov 42, %o0 */
	do_flush(code);
	do_flush(code + 1);
	check("flush", ((int (*)(void))code)() == 42);

	pid = sys_fork();
	if (pid == 0) {
		bss[0] = 12345;
		say("init: child running\n");
		sys_exit(7);
	}
	check("fork", pid > 1);
	check("wait", sys_wait4(pid, &status, 0, 0) == pid);
	check("exit status", (status >> 8) == 7);
	check("copy on write", bss[0] == 0);

	/* A fault must not dump core: there is nowhere to write it yet. */
	sys_setrlimit(RLIMIT_CORE, nocore);
	refused("user read of direct map", 0x90000000, 0);
	refused("user write of direct map", 0x90000000, 1);
	refused("user jump to direct map", 0x90000000, 2);
	refused("user read of registers", 0x60000064, 0);
	refused("user read of kernel window", 0xf8000000, 0);
	refused("user read of bottom of kernel", 0x40000000, 0);
	refused("user read of unmapped page", 0x30000000, 0);

	say("init: done\n");
	sys_execve(shell, shell_argv, shell_envp);
	say("init: no shell\n");
	for (;;)
		sys_wait4(-1, &status, 0, 0);
}
