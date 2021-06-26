#include "process.h"
#include "lock.h"
#include "pagetable.h"
#include "elf.h"
#include "memlayout.h"

extern const char binary_putc_start;
extern char trampoline[];

extern void usertrapret();

void swtch(struct context *, struct context *);

thread_t *running[NCPU];
struct context cpu_context[NCPU];
struct list_head sched_list[NCPU];
struct lock pidlock, tidlock, schedlock;
int _pid, _tid;
process_t proc[NPROC];
thread_t th[NTHREAD * NPROC];


// 将ELF文件映射到给定页表的地址空间，返回pc的数值
// 关于 ELF 文件，请参考：https://docs.oracle.com/cd/E19683-01/816-1386/chapter6-83432/index.html
static uint64 load_binary(pagetable_t target_page_table, const char *bin) {
    struct elf_file *elf;
    int i;
    uint64 seg_sz, p_vaddr, seg_map_sz;
    elf = elf_parse_file(bin);

    /* load each segment in the elf binary */
    for (i = 0; i < elf->header.e_phnum; ++i) {
        if (elf->p_headers[i].p_type == PT_LOAD) {
            // 根据 ELF 文件格式做段映射
            // 从ELF中获得这一段的段大小
            seg_sz = elf->p_headers[i].p_memsz;
            // 对应段的在内存中的虚拟地址
            p_vaddr = elf->p_headers[i].p_vaddr;
            // 对映射大小做页对齐
            seg_map_sz = ROUNDUP(seg_sz + p_vaddr, PGSIZE) - PGROUNDDOWN(p_vaddr);
            // 接下来代码的期望目的：将程序代码映射/复制到对应的内存空间
            // 一种可能的实现如下：
            /* 
             * 在 target_page_table 中分配一块大小
             * 通过 memcpy 将某一段复制进入这一块空间
             * 页表映射修改
             */
            for (int offset = 0; offset < seg_map_sz; offset += PGSIZE) {
                void *tmp = mm_kalloc();
                memset(tmp, 0, PGSIZE);
                uint64 sz = PGSIZE;
                if (seg_map_sz - offset < PGSIZE) sz = seg_map_sz - offset;
                memcpy(tmp, (void *) ((uint64) bin + elf->p_headers[i].p_offset + offset), sz);
                int perm = PTE_U;
                if (elf->p_headers[i].p_flags & 0x1) perm |= PTE_X;
                if (elf->p_headers[i].p_flags & 0x2) perm |= PTE_W;
                if (elf->p_headers[i].p_flags & 0x4) perm |= PTE_R;
                pt_map_pages(target_page_table, p_vaddr + offset, (uint64) tmp, PGSIZE, perm);
            }
        }
    }
    /* PC: the entry point */
    return elf->header.e_entry;
}

/* 分配一个进程，需要至少完成以下目标：
 * 
 * 分配一个主线程
 * 创建一张进程页表
 * 分配pid、tid
 * 设置初始化线程上下文
 * 设置初始化线程返回地址寄存器ra，栈寄存器sp
 * 
 * 这个函数传入参数为一个二进制的代码和一个线程指针(具体传入规则可以自己修改)
 * 此外程序首次进入用户态之前，应该设置好trap处理向量为usertrap（或者你自定义的）
 */
pagetable_t pt_create(process_t *p, thread_t *t) {
    pagetable_t pt = (pagetable_t) mm_kalloc();
    memset(pt, 0, BD_LEAF_SIZE);
    pt_map_pages(pt, TRAMPOLINE, (uint64) trampoline, PGSIZE, PTE_R | PTE_X);
    pt_map_pages(pt, TRAPFRAME(t->kth), (uint64) t->trapframe, PGSIZE, PTE_R | PTE_W);
    pt_map_pages(pt, USTACK(t->kth), (uint64) mm_kalloc(), PGSIZE, PTE_R | PTE_W | PTE_U);
    return pt;
}

process_t *alloc_proc(const char *bin, thread_t **thr) {
    int i;
    for (i = 0; i < NPROC; i++) {
        if (proc[i].process_state == UNUSED) {
            break;
        }
    }
    if (i >= NPROC) return NULL;
    process_t *p = &proc[i];
    int j;
    for (j = i * NTHREAD; j < (i + 1) * NTHREAD; j++) {
        if (th[j].thread_state == UNUSED) {
            break;
        }
    }
    if (j >= (i + 1) * NTHREAD) return NULL;
    thread_t *t = &th[j];
    init_list_head(&p->thread_list);
    list_add(&t->process_list_thread_node, &p->thread_list);
    p->process_state = IDLE;
    p->pid = ++_pid;
    t->thread_state = RUNNABLE;
    t->tid = ++_tid;
    t->kth = j - i * NTHREAD;
    t->fa = p;
    t->trapframe = (struct trapframe *) mm_kalloc();
    t->trapframe->sp = USTACK(t->kth) + PGSIZE;
    t->kernel_stack_va = KSTACK((int) (t - th));
    p->pagetable = pt_create(p, t);
    memset(&t->context, 0, sizeof(t->context));
    t->context.ra = (uint64) usertrapret;
    t->context.sp = t->kernel_stack_va + PGSIZE;
    t->trapframe->epc = load_binary(p->pagetable, bin);
    *thr = t;
    return p;
}

bool load_thread(file_type_t type) {
    if (type == PUTC) {
        thread_t *t = NULL;
        process_t *p = alloc_proc(&binary_putc_start, &t);
        if (!t) return false;
        sched_enqueue(t);
        return true;
    } else {
        BUG("Not supported");
    }
}

thread_t *get_thread() {
    return running[cpuid()];
}

// sched_enqueue和sched_dequeue的主要任务是加入一个任务到队列中和删除一个任务
// 这两个函数的展示了如何使用list.h中可的函数（加入、删除、判断空、取元素）
// 具体可以参考：Stackoverflow上的回答
// https://stackoverflow.com/questions/15832301/understanding-container-of-macro-in-the-linux-kernel
void sched_enqueue(thread_t *target_thread) {
    if (target_thread->thread_state == RUNNING) BUG("Running Thread cannot be scheduled.");
    list_add(&target_thread->sched_list_thread_node, &(sched_list[cpuid()]));
}

thread_t *sched_dequeue() {
    if (list_empty(&(sched_list[cpuid()]))) BUG("Scheduler List is empty");
    thread_t *head = container_of(sched_list[cpuid()].prev, thread_t, sched_list_thread_node);
    list_del(&head->sched_list_thread_node);
    return head;
}

bool sched_empty() {
    return list_empty(&(sched_list[cpuid()]));
}

// 开始运行某个特定的函数
void thread_run(thread_t *target) {
    if (target->thread_state == RUNNABLE) {
        target->thread_state = RUNNING;
        running[cpuid()] = target;
        swtch(&cpu_context[cpuid()], &target->context);
    }
}

void sched_yield() {
    thread_t *thr = get_thread();
    if (thr->thread_state == RUNNING) thr->thread_state = RUNNABLE;
    running[cpuid()] = 0;
    sched_enqueue(thr);
    swtch(&thr->context, &cpu_context[cpuid()]);
}

// sched_start函数启动调度，按照调度的队列开始运行。
void sched_start() {
    while (1) {
        if (sched_empty()) BUG("Scheduler list empty, no app loaded");
        thread_t *next = sched_dequeue();
        thread_run(next);
    }
}

void sched_init() {
    // 初始化调度队列锁
    lock_init(&schedlock);
    // 初始化队列头
    init_list_head(&(sched_list[cpuid()]));
}

void proc_init() {
    // 初始化pid、tid锁
    lock_init(&pidlock);
    lock_init(&tidlock);
    // 接下来代码期望的目的：映射第一个用户线程并且插入调度队列
    if (!load_thread(PUTC)) BUG("Load failed");
}


