#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdint.h>
#include <unistd.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define noinline __attribute__((noinline))


/*
 * Simple user-space structure.
 */
struct crash_test_data {
	int val;
	char *str;
};


/*
 * Crash test function type.
 */
typedef void (*crash_func_t)(void);


/*
 * Crash test descriptor.
 */
struct crash_test {
	const char *name;
	crash_func_t func;
};


/*
 * ---------------------------------------------------------
 * Crash test implementations
 * ---------------------------------------------------------
 */
#define REC_STACK_SIZE 	1024
static int recur_count = 16383;

static noinline int recursive_loop(int remaining)
{
	volatile char buf[REC_STACK_SIZE];

	memset((void *)buf, remaining & 0xFF, sizeof(buf));

	if (!remaining)
		return 0;

	return recursive_loop(remaining - 1);
}

static void ludtm_EXHAUST_STACK(void)
{
	printf("Calling function with %d-byte frame to depth %d ...\n",
		REC_STACK_SIZE, recur_count);

	recursive_loop(recur_count);

	printf("FAIL: survived without exhausting stack?!\n");
}


static noinline void ludtm_CORRUPT_STACK(void)
{
	char data[8];

	memset((void *)data, 0, 64);
}

static void ludtm_DOUBLE_FREE(void)
{
	struct crash_test_data *ptr;

	ptr = malloc(sizeof(*ptr));

	if (!ptr)
		return;

	printf("Allocated memory at %p\n", (void *)ptr);
	free(ptr);

	printf("Attempting to free the memory again at %p\n",
						(void *)ptr);

	free(ptr);
}

static void ludtm_ABORT(void)
{
        printf("The call to abort() is made\n");
        abort();
}

static void read_sepc(void)
{
	unsigned long value;

	asm volatile(
	    "csrr %0, sepc"
	    : "=r"(value)
	    :
	    : "memory"
	);

	printf("sepc = 0x%lx\n", value);
}

static void ludtm_SIGILL_PRIV(void)
{
	printf("Do ludtm_SIGILL_PRIV\n");
	read_sepc();
}

//__attribute__((noinline))
static void use_vector_instruction(void)
{
	asm volatile(
		"vsetvli t0, zero, e8, m1, ta, ma\n"
		:
		:
		: "t0", "memory"
	);
}

static void ludtm_SIGILL_ISA(void)
{
	printf("Do ludtm_SIGILL_ISA\n");
	use_vector_instruction();
}

static const unsigned long user_rodata = 0xDEADBEEF;

static void ludtm_WRITE_RO(void)
{
	volatile unsigned long *ptr =
		(unsigned long *)&user_rodata;

	printf("Attempting bad user_rodata write at %p\n",
						(void *)ptr);

	*ptr = 0x12345678;
}


static void ludtm_ACCESS_NULL(void)
{
	struct crash_test_data *data = NULL;
	volatile int val;

	val = data->val;
}

static void ludtm_EXCEPTION(void)
{
	*((volatile int *)0xdeadbeef) = 0;
}

/*
 * ---------------------------------------------------------
 * Wrapper functions
 *
 * These wrappers are intentionally marked noinline so that
 * they remain visible in the GDB call stack.
 * ---------------------------------------------------------
 */

static noinline void execute_EXHAUST_STACK(void)
{
	ludtm_EXHAUST_STACK();
}

static noinline void execute_CORRUPT_STACK(void)
{
	ludtm_CORRUPT_STACK();
}

static noinline void execute_DOUBLE_FREE(void)
{
	ludtm_DOUBLE_FREE();
}

static noinline void execute_ABORT(void)
{
	ludtm_ABORT();
}

static noinline void execute_SIGILL_PRIV(void)
{
	ludtm_SIGILL_PRIV();
}

static noinline void execute_SIGILL_ISA(void)
{
	ludtm_SIGILL_ISA();
}

static noinline void execute_WRITE_RO(void)
{
	ludtm_WRITE_RO();
}

static noinline void execute_ACCESS_NULL(void)
{
	ludtm_ACCESS_NULL();
}

static noinline void execute_EXCEPTION(void)
{
	ludtm_EXCEPTION();
}

/*
 * ---------------------------------------------------------
 * Crash test table
 * ---------------------------------------------------------
 */

static const struct crash_test crash_tests[] = {
	{ "EXCEPTION",        execute_EXCEPTION },
	{ "ACCESS_NULL",      execute_ACCESS_NULL },
	{ "WRITE_RO",         execute_WRITE_RO },
	{ "SIGILL_ISA",       execute_SIGILL_ISA },
	{ "SIGILL_PRIV",      execute_SIGILL_PRIV },
	{ "ABORT",            execute_ABORT },
	{ "DOUBLE_FREE",      execute_DOUBLE_FREE },
	{ "CORRUPT_STACK",    execute_CORRUPT_STACK },
	{ "EXHAUST_STACK",    execute_EXHAUST_STACK },
};

/*
 * ---------------------------------------------------------
 * Crash test lookup
 * ---------------------------------------------------------
 */

static const struct crash_test *
find_crash_test(const char *name)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(crash_tests); i++) {
		if (strcmp(crash_tests[i].name, name) == 0)
			return &crash_tests[i];
	}

	return NULL;
}


/*
 * ---------------------------------------------------------
 * Crash test execution
 * ---------------------------------------------------------
 */

static noinline void run_crash_test(const char *name)
{
	const struct crash_test *test;

	test = find_crash_test(name);

	if (!test) {
		printf("Unknown crash test: %s\n", name);
		return;
	}

	test->func();
}

/*
 * ---------------------------------------------------------
 * Entry point for crash test
 * ---------------------------------------------------------
 */

static noinline void make_coredump(char *buf)
{
	printf("Do %s\n", buf);

	run_crash_test(buf);
}


int main(int argc, char *argv[])
{
	if (argc < 2) {
		printf("Usage: %s <crash_test>\n", argv[0]);
		return EXIT_FAILURE;
	}

	make_coredump(argv[1]);

	return EXIT_SUCCESS;
}
