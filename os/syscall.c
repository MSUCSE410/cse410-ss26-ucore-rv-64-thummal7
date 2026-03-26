#include "syscall.h"
#include "defs.h"
#include "loader.h"
#include "syscall_ids.h"
#include "timer.h"
#include "trap.h"
#include "proc.h"
#include "vm.h"

uint64 sys_write(int fd, char *str, uint len)
{
    struct proc *p = curr_proc();
    char ch;

    debugf("sys_write fd = %d str = %x, len = %d", fd, str, len);

    if (fd != STDOUT)
        return -1;

    for (uint i = 0; i < len; ++i) {
        if (copyin(p->pagetable, &ch, (uint64)str + i, 1) < 0)
            return -1;
        console_putchar(ch);
    }

    return len;
}

__attribute__((noreturn)) void sys_exit(int code)
{
    exit(code);
    __builtin_unreachable();
}

uint64 sys_sched_yield()
{
    yield();
    return 0;
}
/////
uint64 sys_gettimeofday(TimeVal *val, int _tz)
{
    struct proc *p = curr_proc();

    uint64 pa = useraddr(p->pagetable, (uint64)val);
    if (pa == 0)
        return -1;

    TimeVal *tv = (TimeVal *)pa;

    uint64 cycle = get_cycle();
    tv->sec = cycle / CPU_FREQ;
    tv->usec = (cycle % CPU_FREQ) * 1000000 / CPU_FREQ;
    return 0;
}
/////

//////
uint64 sys_task_info(TaskInfo *ti)
{
    struct proc *p = curr_proc();

    uint64 pa = useraddr(p->pagetable, (uint64)ti);
    if (pa == 0)
        return -1;

    TaskInfo *info = (TaskInfo *)pa; 

    info->status = Running;
	/////
	
    for (int i = 0; i < MAX_SYSCALL_NUM; i++) {
        info->syscall_times[i] = p->syscall_times[i];
    }

    uint64 now = r_time();
    if (p->started == 0) {
		/////
        info->time = 0;
    } else {
        uint64 diff = now - p->start_cycle;
        info->time = diff * 1000 / CPU_FREQ;
    }/////

    return 0;
}

/////
uint64 sys_mmap(uint64 start, uint64 len, int port, int flag, int fd)
{
    struct proc *p = curr_proc();

    if (len == 0)
        return 0;

    if (len > (1ULL << 30))
        return -1;

    if ((port & ~0x7) != 0)
        return -1;

    if ((port & 0x7) == 0)
        return -1;
    if (start % PAGE_SIZE != 0)  
        return -1;
    len = PGROUNDUP(len);


    int perm = PTE_U;
    if (port & 1) perm |= PTE_R;
    if (port & 2) perm |= PTE_W;
    if (port & 4) perm |= PTE_X;

    if (vm_mmap(p->pagetable, start, len, perm) < 0)
        return -1;

    return 0;
}

uint64 sys_munmap(uint64 start, uint64 len)
{
    if (len == 0)
        return 0;
    if (start % PAGE_SIZE != 0)
        return -1;
    if (len % PAGE_SIZE != 0)
        return -1;

    struct proc *p = curr_proc();
    if (vm_munmap(p->pagetable, start, len) < 0)
        return -1;

    return 0;
}

/////

// TODO: add support for mmap and munmap syscall.
// hint: read through docstrings in vm.c. Watching CH4 video may also help.
// Note the return value and PTE flags (especially U,X,W,R)
/*
* LAB1: you may need to define sys_task_info here
*/

extern char trap_page[];

uint64 sys_getpid()
{
    return curr_proc()->pid;
}

void syscall()
{
    struct trapframe *trapframe = curr_proc()->trapframe;
    int id = trapframe->a7, ret;
    uint64 args[6] = { trapframe->a0, trapframe->a1, trapframe->a2,
               trapframe->a3, trapframe->a4, trapframe->a5 };
    tracef("syscall %d args = [%x, %x, %x, %x, %x, %x]", id, args[0],
           args[1], args[2], args[3], args[4], args[5]);
    /*
    * LAB1: you may need to update syscall counter for task info here
    */
    if (id >= 0 && id < MAX_SYSCALL_NUM) {
        curr_proc()->syscall_times[id]++;
    }


    switch (id) {
    case SYS_write:
        ret = sys_write(args[0], (char *)args[1], args[2]);
        break;
    case SYS_exit:
        sys_exit(args[0]);
        // __builtin_unreachable();
    case SYS_sched_yield:
        ret = sys_sched_yield();
        break;
    case SYS_gettimeofday:
        ret = sys_gettimeofday((TimeVal *)args[0], args[1]);
        break;

	case SYS_mmap:
		ret = sys_mmap(args[0], args[1], args[2], args[3], args[4]);
		break;
/////
	case SYS_munmap:
		ret = sys_munmap(args[0], args[1]);
		break;
        /////

	case SYS_getpid:
		ret = sys_getpid();
		break;
    /*
    * LAB1: you may need to add SYS_taskinfo case here
    */
    case SYS_task_info:
        ret = sys_task_info((TaskInfo *)args[0]);
        break;

    default:
        ret = -1;
        errorf("unknown syscall %d", id);
    }
    trapframe->a0 = ret;
    tracef("syscall ret %d", ret);
}


