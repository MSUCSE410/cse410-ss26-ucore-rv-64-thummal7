#include "proc.h"
#include "defs.h"
#include "loader.h"
#include "trap.h"
#include "vm.h"

struct proc pool[NPROC];
char kstack[NPROC][PAGE_SIZE];
__attribute__((aligned(4096))) char ustack[NPROC][PAGE_SIZE];
__attribute__((aligned(4096))) char trapframe[NPROC][PAGE_SIZE];

extern char boot_stack_top[];
extern char trampoline[];
struct proc *current_proc;
struct proc idle;

int threadid()
{
    return curr_proc()->pid;
}

struct proc *curr_proc()
{
    return current_proc;
}

// initialize the proc table at boot time.
void proc_init(void)
{
    struct proc *p;
    for (p = pool; p < &pool[NPROC]; p++) {
        p->state = UNUSED;
        p->kstack = (uint64)kstack[p - pool];
        p->ustack = (uint64)ustack[p - pool];
        p->trapframe = (struct trapframe *)trapframe[p - pool];
        /*
        * LAB1: you may need to initialize your new fields of proc here
        */
        p->started = 0;
        p->start_cycle = 0;
        memset(p->syscall_times, 0, sizeof(p->syscall_times));

    }
    idle.kstack = (uint64)boot_stack_top;
    idle.pid = 0;
    current_proc = &idle;
}

int allocpid()
{
    static int PID = 1;
    return PID++;
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel.
// If there are no free procs, or a memory allocation fails, return 0.
struct proc *allocproc(void)
{
    struct proc *p;
    for (p = pool; p < &pool[NPROC]; p++) {
        if (p->state == UNUSED) {
            goto found;
        }
    }
    return 0;

found:
    p->pid = allocpid();
    p->state = USED;
    memset(&p->context, 0, sizeof(p->context));
    memset(p->trapframe, 0, PAGE_SIZE);
    memset((void *)p->kstack, 0, PAGE_SIZE);

    p->pagetable = uvmcreate();
    if (p->pagetable == 0) {
        p->state = UNUSED;
        return 0;
    }

    if (mappages(p->pagetable, TRAPFRAME, PAGE_SIZE, (uint64)p->trapframe, PTE_R | PTE_W) < 0) {
        uvmfree(p->pagetable, 0);
        p->pagetable = 0;
        p->state = UNUSED;
        return 0;
    }

if (mappages(p->pagetable, TRAPFRAME - PAGE_SIZE, PAGE_SIZE, (uint64)p->ustack, PTE_R | PTE_W | PTE_U) < 0) {
        uvmunmap(p->pagetable, TRAPFRAME, 1, 0);
        uvmfree(p->pagetable, 0);
        p->pagetable = 0;
        p->state = UNUSED;
        return 0;
    }

    p->context.ra = (uint64)usertrapret;
    p->context.sp = p->kstack + PAGE_SIZE;
    return p;
}
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void scheduler(void)
{
    struct proc *p;
    for (;;) {
        for (p = pool; p < &pool[NPROC]; p++) {
            if (p->state == RUNNABLE) {
                /*
                * LAB1: you may need to init proc start time here
                */
                if (p->started == 0) {
                    p->started = 1;
                    p->start_cycle = r_time();
                }

                p->state = RUNNING;
                current_proc = p;
                swtch(&idle.context, &p->context);
            }
        }
    }
}

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void sched(void)
{
    struct proc *p = curr_proc();
    if (p->state == RUNNING)
        panic("sched RUNNING");
    swtch(&p->context, &idle.context);
}

// Give up the CPU for one scheduling round.
void yield(void)
{
    current_proc->state = RUNNABLE;
    sched();
}

void freeproc(struct proc *p)
{
    if (p->pagetable) {
        uvmunmap(p->pagetable, TRAMPOLINE, 1, 0);
        uvmunmap(p->pagetable, TRAPFRAME, 1, 0);
        uvmunmap(p->pagetable, TRAPFRAME - PAGE_SIZE, 1, 0);
        uint64 app_va = BASE_ADDRESS + p->app_id * MAX_APP_SIZE;
		        printf("freeproc: unmapping app_id=%d va=%p npages=%d\n", 
               p->app_id, app_va, MAX_APP_SIZE / PAGE_SIZE);

        uvmunmap(p->pagetable, app_va, MAX_APP_SIZE / PAGE_SIZE, 0);
		        uvmunmap_mmap_pages(p->pagetable);

        uvmfree(p->pagetable, 0);
        p->pagetable = 0;
    }
    p->state = UNUSED;
}
// Exit the current process.
void exit(int code)
{
    struct proc *p = curr_proc();
    infof("proc %d exit with %d", p->pid, code);
	freeproc(p);
	finished();
	sched();
}
