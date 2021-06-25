//
// Created by Wenxin Zheng on 2021/3/5.
//

#ifndef ACMOS_SPR21_PROCESS_H
#define ACMOS_SPR21_PROCESS_H

#include <list.h>
#include <pagetable.h>

typedef enum state { UNUSED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE, IDLE } process_state_t;
typedef enum file_type{PUTC} file_type_t;

// Saved registers for kernel context switches.
struct context {
    uint64 ra;
    uint64 sp;

    // callee-saved
    uint64 s0;
    uint64 s1;
    uint64 s2;
    uint64 s3;
    uint64 s4;
    uint64 s5;
    uint64 s6;
    uint64 s7;
    uint64 s8;
    uint64 s9;
    uint64 s10;
    uint64 s11;
};

struct trapframe {
    /*   0 */ uint64 kernel_satp;   // kernel page table
    /*   8 */ uint64 kernel_sp;     // top of process's kernel stack
    /*  16 */ uint64 kernel_trap;   // usertrap()
    /*  24 */ uint64 epc;           // saved user program counter
    /*  32 */ uint64 kernel_hartid; // saved kernel tp
    /*  40 */ uint64 ra;
    /*  48 */ uint64 sp;
    /*  56 */ uint64 gp;
    /*  64 */ uint64 tp;
    /*  72 */ uint64 t0;
    /*  80 */ uint64 t1;
    /*  88 */ uint64 t2;
    /*  96 */ uint64 s0;
    /* 104 */ uint64 s1;
    /* 112 */ uint64 a0;
    /* 120 */ uint64 a1;
    /* 128 */ uint64 a2;
    /* 136 */ uint64 a3;
    /* 144 */ uint64 a4;
    /* 152 */ uint64 a5;
    /* 160 */ uint64 a6;
    /* 168 */ uint64 a7;
    /* 176 */ uint64 s2;
    /* 184 */ uint64 s3;
    /* 192 */ uint64 s4;
    /* 200 */ uint64 s5;
    /* 208 */ uint64 s6;
    /* 216 */ uint64 s7;
    /* 224 */ uint64 s8;
    /* 232 */ uint64 s9;
    /* 240 */ uint64 s10;
    /* 248 */ uint64 s11;
    /* 256 */ uint64 t3;
    /* 264 */ uint64 t4;
    /* 272 */ uint64 t5;
    /* 280 */ uint64 t6;
};

typedef struct process {
    struct list_head thread_list;
    process_state_t process_state;
    // 以下部分请根据自己的需要自行填充
    int pid;
    pagetable_t pagetable;
} process_t;

// 状态可以根据自己的需要进行修改
typedef process_state_t thread_state_t;

typedef struct thread {
    struct list_head process_list_thread_node;
    thread_state_t thread_state;
    struct list_head sched_list_thread_node;
    // 以下部分请根据自己的需要自行填充
    process_t *fa;
    int tid;
    int kth;
    uint64 kernel_stack_va;
    struct trapframe *trapframe;
    struct context context;
} thread_t;

process_t *alloc_proc(const char* bin, thread_t **thr);
bool load_thread(file_type_t type);
thread_t *get_thread();
void sched_enqueue(thread_t *target_thread);
thread_t *sched_dequeue();
bool sched_empty();
void sched_yield();
void sched_start();
void sched_init();
void proc_init();
void trap_init_vec();
#endif  // ACMOS_SPR21_PROCESS_H
