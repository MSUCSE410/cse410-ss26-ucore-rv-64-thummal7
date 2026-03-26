#include "loader.h"
#include "defs.h"
#include "trap.h"

static uint64 app_num;
static uint64 *app_info_ptr;
extern char _app_num[], ekernel[];

// Count finished programs. If all apps exited, shutdown.
int finished()
{
    static int fin = 0;
    if (++fin >= app_num)
        panic("all apps over");
    return 0;
}

// Get user progs' infomation through pre-defined symbol in `link_app.S`
void loader_init()
{

    app_info_ptr = (uint64 *)_app_num;
    app_num = *app_info_ptr;
    app_info_ptr++;
}

// Load nth user app at
// [BASE_ADDRESS + n * MAX_APP_SIZE, BASE_ADDRESS + (n+1) * MAX_APP_SIZE)
int load_app(int n, uint64 *info)
{
    uint64 start = info[n], end = info[n + 1], length = end - start;
    memset((void *)BASE_ADDRESS + n * MAX_APP_SIZE, 0, MAX_APP_SIZE);
    memmove((void *)BASE_ADDRESS + n * MAX_APP_SIZE, (void *)start, length);
    return length;
}

// load all apps and init the corresponding `proc` structure.
int run_all_app()
{
    for (int i = 0; i < app_num; ++i) {
        struct proc *p = allocproc();
		p->app_id = i; 
        struct trapframe *trapframe = p->trapframe;
        load_app(i, app_info_ptr);
        uint64 entry = BASE_ADDRESS + i * MAX_APP_SIZE;
        tracef("load app %d at %p", i, entry);

        if (mappages(p->pagetable, entry, MAX_APP_SIZE,
                     entry, PTE_R | PTE_W | PTE_X | PTE_U) < 0) {
            panic("run_all_app: mappages failed");
        }

        trapframe->epc = entry;
        trapframe->sp = TRAPFRAME; 
        p->state = RUNNABLE;

        p->started = 0;
        p->start_cycle = 0;
        memset(p->syscall_times, 0, sizeof(p->syscall_times));
    }
    return 0;
}