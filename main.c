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
    // int tid;
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

void *test_routine(void *arg)
{
    (void)arg;
    printf("bonjour\n");
    sleep(1);
    return NULL;
}

int main(void)
{
    pthreat_t thread;
    pthreat_create(&thread, test_routine, NULL);
    pthreat_join(thread, NULL);

    return 0;
}
