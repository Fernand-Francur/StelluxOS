#ifndef PROCESS_H
#define PROCESS_H
#include "ptregs.h"
#include <arch/percpu.h>

#define MAX_PROCESS_NAME_LEN 255

typedef int64_t pid_t;

enum class process_state {
    INVALID = 0, // Process doesn't exist
    NEW,        // Process created but not yet ready to run
    READY,      // Ready to be scheduled
    RUNNING,    // Currently executing
    WAITING,    // Waiting for some resource
    TERMINATED  // Finished execution
};

struct task_control_block {
    ptregs          cpu_context;
    process_state   state;
    pid_t           pid;
    uint64_t        kernel_stack;
    uint64_t        kernel_interrupt_stack;
    uint64_t        user_stack;

    struct {
        uint64_t    elevated    : 1;
        uint64_t    cpu         : 8;
        uint64_t    flrsvd      : 55;
    } __attribute__((packed));

    char            name[MAX_PROCESS_NAME_LEN + 1];
};

DECLARE_PER_CPU(task_control_block*, current_task);
DECLARE_PER_CPU(uintptr_t, default_kernel_stack);
DECLARE_PER_CPU(uintptr_t, current_kernel_stack);
DECLARE_PER_CPU(uintptr_t, current_user_stack);

static __force_inline__ task_control_block* get_current_task() {
    return this_cpu_read(current_task);
}

#define current get_current_task()

#endif // PROCESS_H

