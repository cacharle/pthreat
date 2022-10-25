#define _GNU_SOURCE
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <sched.h>
#include <signal.h>
#include <assert.h>

#define STACK_SIZE (1024 * 64)  // 64 Mb

#define ASSERT(x) if (!(x)) { \
    perror(NULL); \
    exit(1); \
}

typedef struct pthreat {
    pid_t pid;
    void *ret;
} pthreat_t;

typedef void *(*pthreat_routine_t)(void*);

struct routine_wrapper_args {
    pthreat_t *thread;
    pthreat_routine_t routine;
    void *routine_arg;
    void **return_addr;
};

int routine_wrapper(void *void_args)
{
    struct routine_wrapper_args *args = void_args;
    args->thread->ret = args->routine(args->routine_arg);
    free(args);
    return 0;
}

int pthreat_create(pthreat_t *thread, pthreat_routine_t routine, void *arg)
{
    void *stack = malloc(sizeof(pthreat_t) + STACK_SIZE);
    ASSERT(stack != NULL);
    struct routine_wrapper_args *wrapper_args = malloc(sizeof(struct routine_wrapper_args));
    ASSERT(wrapper_args != NULL);
    wrapper_args->routine = routine;
    wrapper_args->routine_arg = arg;
    wrapper_args->return_addr = NULL;
    thread->ret = NULL;
    pid_t pid = clone(
        routine_wrapper,
        (uint8_t*)stack + STACK_SIZE,
        SIGCHLD |        // Signal sent to the parent when child exits
        CLONE_FS |       // Share filesystem info (e.g current working directory, umask, ...)
        CLONE_FILES |    // Share file descriptors table
        CLONE_SIGHAND |  // Share signal handling
        CLONE_VM,        // Share virtual memory
        wrapper_args
    );
    ASSERT(pid != -1);
    thread->pid = pid;
    return 0;
}

int pthreat_join(pthreat_t thread, void **ret)
{
    waitpid(thread.pid, 0, 0);
    if (ret != NULL)
        *ret = thread.ret;
    return 0;
}

int pthreat_kill(pthreat_t thread, int sig)
{
    return kill(thread.pid, sig);
}

// in musl a_cas
#define atomic_compare_and_swap atomic_compare_and_swap
static inline int atomic_compare_and_swap(volatile int *dest, int test, int source)
{
    // __asm__ <- inline assembly
    // __volatile__ <- prevent compiler optimisation
    // __asm__ (
    //     actual assembly code in a string
    //     : output values
    //     : input values
    //     : clobbered values (memory which will me modified by the instructions)
    // )
    // input/output values format:
    //   "modifiers" (c variable)
    //   you can access the c variables in assembly with %[number]
    //   "r" to put variable in register
    //   "=r" to put variable in register and mark it as write only
    //   "m" to put variable in memory
    //   "a" to put variable in register rax
    //
    // the `lock` innstruction makes things atomic
    //
    // `cmpxchg` compare the dest operand with rax,
    // if equal,
    //   the source operand is loaded into the dest operand
    //   otherwise, the dest operand is loaded in rax
    __asm__ __volatile__ (
        "lock ; cmpxchg %3, %1"
        : "=a" (test), "=m" (*dest)
        : "a" (test), "r" (source)
        : "memory"
    );
    return test;
}

typedef struct {
    int locked;
} pthreat_mutex_t;

int pthreat_mutex_init(pthreat_mutex_t *mutex /* attr */)
{
    mutex->locked = 0;
    return 0;
}

int pthreat_mutex_destroy(pthreat_mutex_t *mutex)
{
    return 0;
}

int pthreat_mutex_lock(pthreat_mutex_t *mutex)
{
    // BRU!, so beautiful i m crying and masturbating at the same time
    while (atomic_compare_and_swap(&mutex->locked, 0, 1))
        ;
    assert(mutex->locked);
    return 0;
}

int pthreat_mutex_unlock(pthreat_mutex_t *mutex)
{
    // assert(mutex->locked);
    mutex->locked = 0;
}

pthreat_mutex_t mutex;

static int max_value = 10000;
static int routine_total = 0;
static int regular_total = 0;

void *test_routine(void *arg)
{
    (void)arg;
    for (int i = 0; i < max_value; i++)
    {
        pthreat_mutex_lock(&mutex);
        routine_total += i;
        pthreat_mutex_unlock(&mutex);
    }
    return NULL;
}

int main(void)
{
    pthreat_mutex_init(&mutex);
    pthreat_t thread1;
    pthreat_t thread2;
    for (int i = 0; i < max_value; i++)
        regular_total += i;
    for (int i = 0; i < max_value; i++)
        regular_total += i;
    pthreat_create(&thread1, test_routine, NULL);
    pthreat_create(&thread2, test_routine, NULL);
    pthreat_join(thread1, NULL);
    pthreat_join(thread2, NULL);
    printf("expected: %d\n", regular_total);
    printf("actual:   %d\n", routine_total);
    return 0;
}
