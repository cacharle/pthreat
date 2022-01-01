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

#define PIPE_READ 0
#define PIPE_WRITE 0


typedef struct pthreat {
    struct pthreat *next;
    struct pthreat *prev;
    pid_t pid;
    // int tid;
} pthreat_t;

// typedef struct pthreat_desc pthreat_t;

static pthreat_t *manager_g = NULL;
static int pthreat_manager_pipe_g[2] = {-1, -1};

enum manager_request_type {
    MANAGER_REQUEST_CREATE,
};

struct manager_request {
    enum manager_request_type type;

};

void *manager_routine(void *arg)
{
    while (true)
    {
        struct manager_request request;
        size_t read_size = read(pthreat_manager_pipe_g[PIPE_READ], &request, sizeof(request));
        assert(read_size != sizeof(request));
        switch (request.type)
        {
            case MANAGER_REQUEST_CREATE:
                printf("foo\n");
                break;
            default:
                exit(1);
        }
    }
    return NULL;
}

typedef void *(*pthreat_routine_t)(void*);

struct routine_wrapper_args {
    pthreat_routine_t routine;
    void *routine_arg;
    void **return_addr;
};

int routine_wrapper(void *void_args)
{
    struct routine_wrapper_args *args = void_args;
    args->routine(args->routine_arg);
    return 0;
}

static int create_thread(pthreat_t *thread, pthreat_routine_t routine, void *arg)
{
    void *stack = malloc(sizeof(pthreat_t) + STACK_SIZE);
    ASSERT(stack != NULL);

    struct routine_wrapper_args wrapper_args = {
        .routine = routine,
        .routine_arg = arg,
        .return_addr = NULL,
    };

    pid_t pid = clone(
        routine_wrapper,
        (uint8_t*)stack + STACK_SIZE,
        SIGCHLD |        // Signal sent to the parent when child exits
        CLONE_FS |       // Share filesystem info (e.g current working directory, umask, ...)
        CLONE_FILES |    // Share file descriptors table
        CLONE_SIGHAND |  // Share signal handling
        CLONE_VM,        // Share virtual memory
        &wrapper_args
    );
    ASSERT(pid != -1);
    thread->pid = pid;
    thread->next = manager_g->next;
    thread->prev = manager_g;
    manager_g->next->prev = thread;
    manager_g->next = thread;

    return 0;
}

int pthreat_create(
    pthreat_t *thread,
    /* attr, */
    pthreat_routine_t routine,
    void *arg
)
{
    if (manager_g == NULL)
    {
        create_thread(manager_g, manager_routine, arg);
    }
    return 0;
}


void *start_routine(void *arg)
{
    (void)arg;
    printf("bonjour\n");
    return NULL;
}



int main(void)
{
    // printf("yo\n");
    // pid = waitpid(pid, 0, 0);
    // ASSERT(pid != -1)
    // free(stack);
    return 0;
}
