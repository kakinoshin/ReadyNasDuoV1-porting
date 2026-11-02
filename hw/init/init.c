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

#define	O_RDWR		2
#define	PROT_RWX	7
#define	MAP_PRIVATE	0x0002
#define	MAP_ANON	0x1000

static int bss[4096];

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

/* Deep enough to spill every window to the user stack and bring it back. */
static int __attribute__((noinline))
depth(int n, int a, int b)
{

	if (n == 0)
		return a + b;
	return depth(n - 1, b, a + 1) + 1;
}

int
main(void)
{
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

	say("init: done\n");
	for (;;)
		sys_wait4(-1, &status, 0, 0);
}
