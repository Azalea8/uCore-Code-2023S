#include "syscall.h"
#include "defs.h"
#include "loader.h"
#include "syscall_ids.h"
#include "timer.h"
#include "trap.h"
#include "proc.h"

uint64 sys_write(int fd, uint64 va, uint len)
{
	debugf("sys_write fd = %d va = %x, len = %d", fd, va, len);
	if (fd != STDOUT)
		return -1;
	struct proc *p = curr_proc();
	char str[MAX_STR_LEN];
	int size = copyinstr(p->pagetable, str, va, MIN(len, MAX_STR_LEN));
	debugf("size = %d", size);
	for (int i = 0; i < size; ++i) {
		console_putchar(str[i]);
	}
	return size;
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

uint64 sys_gettimeofday(TimeVal *val, int _tz) // TODO: implement sys_gettimeofday in pagetable. (VA to PA)
{
	// YOUR CODE
	struct proc *p = curr_proc();
	TimeVal kernel_val;
	TimeVal* dst = &kernel_val;
	copyin(p -> pagetable, (char *)dst, (uint64)val, sizeof(TimeVal));
	
	uint64 cycle = get_cycle();
	dst->sec = cycle / CPU_FREQ;
	dst->usec = (cycle % CPU_FREQ) * 1000000 / CPU_FREQ;

	copyout(p -> pagetable, (uint64)val, (char *)dst, sizeof(TimeVal));
	/* The code in `ch3` will leads to memory bugs*/

	// uint64 cycle = get_cycle();
	// val->sec = cycle / CPU_FREQ;
	// val->usec = (cycle % CPU_FREQ) * 1000000 / CPU_FREQ;
	return 0;
}

uint64 sys_sbrk(int n)
{
	uint64 addr;
        struct proc *p = curr_proc();
        addr = p->program_brk;
        if(growproc(n) < 0)
                return -1;
        return addr;	
}



// TODO: add support for mmap and munmap syscall.
// hint: read through docstrings in vm.c. Watching CH4 video may also help.
// Note the return value and PTE flags (especially U,X,W,R)
/*
* LAB1: you may need to define sys_task_info here
*/
int sys_task_info(TaskInfo *ti){
	struct proc *p = curr_proc();
	TaskInfo kernel_val;
	TaskInfo* dst = &kernel_val;
	copyin(p -> pagetable, (char *)dst, (uint64)ti, sizeof(TaskInfo));
	
	dst -> status = p -> taskinfo.status;
	for(int i = 0;i < MAX_SYSCALL_NUM;i++) {
		dst -> syscall_times[i] = p -> taskinfo.syscall_times[i];
	}
	dst -> time = get_time() - (p -> taskinfo.time);

	copyout(p -> pagetable, (uint64)ti, (char *)dst, sizeof(TaskInfo));
	return 0;
}

extern char trap_page[];

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
	struct proc *p = curr_proc();
	p -> taskinfo.syscall_times[id]++;

	switch (id) {
	case SYS_write:
		ret = sys_write(args[0], args[1], args[2]);
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
	case SYS_sbrk:
		ret = sys_sbrk(args[0]);
		break;
	/*
	* LAB1: you may need to add SYS_taskinfo case here
	*/
	case SYS_TASK_INFO:
			ret = sys_task_info((TaskInfo *)args[0]);
			break;
	default:
		ret = -1;
		errorf("unknown syscall %d", id);
	}
	trapframe->a0 = ret;
	tracef("syscall ret %d", ret);
}
