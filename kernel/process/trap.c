#include <defs.h>
#include <riscv.h>
#include "process.h"
#include "memlayout.h"

extern char trampoline[], usertrap1[], usertrap2[];

extern void kernelvec();

extern int devintr();

void usertrapret();

void trap_init_vec() {
    // Only 1 LoC
    // 将kernelvec作为内核中断处理基地址写入stvec向量。
    w_stvec((uint64) kernelvec);
}

// 真实的 syscall 处理过程
// 根据你在 user/stdlib.h 中的 syscall 操作在这里对应地寻找目标
void syscall(thread_t *t) {
    if (t->trapframe->a7 == 1) {
        t->thread_state = ZOMBIE;
        sched_yield();
    } else if (t->trapframe->a7 == 2) {
        printk("%c", t->trapframe->a0);
    } else if (t->trapframe->a7 == 3) {
        sched_yield();
    }
}

// 用户态的trap处理函数，内核态请参考 kernel/boot/start.c 中的 kernelvec
void usertrap(void) {
    int which_dev = 0;
    if ((r_sstatus() & SSTATUS_SPP) != 0) BUG("usertrap: not from user mode");
    // 由于中断处理过程可能仍然被中断，所以设置中断处理向量为 kernelvec（内核态处理）
    w_stvec((uint64) kernelvec);
    /* 你需要在这个函数中做的事情（仅供参考，你可以有自己的设计，如果不是2阶段中断处理）：
     * 
     * 保存用户态的pc
     * 判断发生trap的类型：syscall、设备中断、时钟中断等
     * 完成处理，进入到trap后半部分处理函数
     */
    thread_t *t = get_thread();
    t->trapframe->epc = r_sepc();
    if (r_scause() == 8) {
        // system call
        t->trapframe->epc += 4;
        intr_on();
        syscall(t);
    } else if ((which_dev = devintr()) != 0) {
        // ok
    } else {
        printk("usertrap(): unexpected scause %p\n", r_scause());
        BUG_FMT("            sepc=%p stval=%p\n", r_sepc(), r_stval());
    }


    // 处理时钟中断：重新调度
    if (which_dev == 2) sched_yield();
    // 进入 trap 后半处理函数
    usertrapret();
}

// Trap 后半处理函数
void usertrapret() {
    /* 你需要在这个函数中做的事情（仅供参考，你可以有自己的设计）：
     * 
     * 关闭中断
     * 重新设置程序的中断处理向量
     * 还原目标寄存器
     * 设置下一次进入内核的一些参数
     * 切换页表
     * 跳转到二进制代码还原现场的部分
     */
    intr_off();

    w_stvec(TRAMPOLINE + (usertrap1 - trampoline));

    thread_t *t = get_thread();
    t->trapframe->kernel_satp = r_satp();
    t->trapframe->kernel_sp = t->kernel_stack_va + PGSIZE;
    t->trapframe->kernel_trap = (uint64) usertrap;
    t->trapframe->kernel_hartid = r_tp();

    w_sstatus((r_sstatus() & (~SSTATUS_SPP)) | SSTATUS_SPIE);
    w_sepc(t->trapframe->epc);

    uint64 satp = MAKE_SATP(t->fa->pagetable);
    uint64 fn = TRAMPOLINE + (usertrap2 - trampoline);
    ((void (*)(uint64, uint64)) fn)(TRAPFRAME(t->kth), satp);
}