#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;
  struct proc *p = myproc();

  if(argint(0, &n) < 0)
    return -1;
  addr = p->sz;

  if (n < 0)
  {
    // Deallocate memory for negative sbrk; avoid unsigned underflow.
    uint64 new_sz;
    uint64 shrink = (uint64)(-n);
    if (shrink > p->sz)
      new_sz = 0;
    else
      new_sz = p->sz - shrink;
    p->sz = uvmdealloc(p->pagetable, p->sz, new_sz);
  }
  else if (n > 0)
  {
    // Prevent growing into TRAPFRAME/TRAMPOLINE area.
    // User memory must always be strictly below TRAPFRAME.
    uint64 add = (uint64)n;
    if (add > (TRAPFRAME > p->sz ? (TRAPFRAME - p->sz) : 0))
      return (uint64)-1; // fail if it would exceed user address space limit
    // Lazy allocation: just record the size increase.
    p->sz += add;
  }

  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
