#include <assert.h>
#include <clock.h>
#include <console.h>
#include <defs.h>
#include <kdebug.h>
#include <memlayout.h>
#include <mmu.h>
#include <riscv.h>
#include <sbi.h>
#include <stdio.h>
#include <trap.h>
#include <vmm.h>

#define TICK_NUM 100
#define SHUTDOWN_AFTER 10

static void print_ticks()
{
    cprintf("%d ticks\n", TICK_NUM);
#ifdef DEBUG_GRADE
    cprintf("End of Test.\n");
    panic("EOT: kernel seems ok.");
#endif
}

/* idt_init - initialize IDT to each of the entry points in kern/trap/vectors.S
 */
void idt_init(void)
{
    extern void __alltraps(void);
    /* Set sscratch register to 0, indicating to exception vector that we are
     * presently executing in the kernel */
    write_csr(sscratch, 0);
    /* Set the exception vector address */
    write_csr(stvec, &__alltraps);
    /* Allow kernel to access user memory */
    set_csr(sstatus, SSTATUS_SUM);
}

void print_trapframe(struct trapframe *tf)
{
    cprintf("trapframe at %p\n", tf);
    print_regs(&tf->gpr);
    cprintf("  status   0x%08x\n", tf->status);
    cprintf("  epc      0x%08x\n", tf->epc);
    cprintf("  badvaddr 0x%08x\n", tf->badvaddr);
    cprintf("  cause    0x%08x\n", tf->cause);
}

void print_regs(struct pushregs *gpr)
{
    cprintf("  zero     0x%08x\n", gpr->zero);
    cprintf("  ra       0x%08x\n", gpr->ra);
    cprintf("  sp       0x%08x\n", gpr->sp);
    cprintf("  gp       0x%08x\n", gpr->gp);
    cprintf("  tp       0x%08x\n", gpr->tp);
    cprintf("  t0       0x%08x\n", gpr->t0);
    cprintf("  t1       0x%08x\n", gpr->t1);
    cprintf("  t2       0x%08x\n", gpr->t2);
    cprintf("  s0       0x%08x\n", gpr->s0);
    cprintf("  s1       0x%08x\n", gpr->s1);
    cprintf("  a0       0x%08x\n", gpr->a0);
    cprintf("  a1       0x%08x\n", gpr->a1);
    cprintf("  a2       0x%08x\n", gpr->a2);
    cprintf("  a3       0x%08x\n", gpr->a3);
    cprintf("  a4       0x%08x\n", gpr->a4);
    cprintf("  a5       0x%08x\n", gpr->a5);
    cprintf("  a6       0x%08x\n", gpr->a6);
    cprintf("  a7       0x%08x\n", gpr->a7);
    cprintf("  s2       0x%08x\n", gpr->s2);
    cprintf("  s3       0x%08x\n", gpr->s3);
    cprintf("  s4       0x%08x\n", gpr->s4);
    cprintf("  s5       0x%08x\n", gpr->s5);
    cprintf("  s6       0x%08x\n", gpr->s6);
    cprintf("  s7       0x%08x\n", gpr->s7);
    cprintf("  s8       0x%08x\n", gpr->s8);
    cprintf("  s9       0x%08x\n", gpr->s9);
    cprintf("  s10      0x%08x\n", gpr->s10);
    cprintf("  s11      0x%08x\n", gpr->s11);
    cprintf("  t3       0x%08x\n", gpr->t3);
    cprintf("  t4       0x%08x\n", gpr->t4);
    cprintf("  t5       0x%08x\n", gpr->t5);
    cprintf("  t6       0x%08x\n", gpr->t6);
}

extern struct mm_struct *check_mm_struct;

//lab3 请补充你在lab3中的代码

bool trap_in_kernel(struct trapframe *tf) {
    return (tf->status & SSTATUS_SPP) != 0;
}

void interrupt_handler(struct trapframe *tf) {
    const uintptr_t interrupt_bit = (uintptr_t)1 << (sizeof(tf->cause) * 8 - 1);
    uintptr_t cause = tf->cause & ~interrupt_bit;

    switch (cause) {
    case IRQ_S_TIMER:
        /* Re-arm the timer before printing or returning from this trap. */
        clock_set_next_event();
        ++ticks;
        if (ticks % TICK_NUM == 0) {
            print_ticks();
            /* Divide rather than multiply to avoid a threshold overflow. */
            if (ticks / TICK_NUM >= SHUTDOWN_AFTER) {
                sbi_shutdown();
                /* Do not resume the kernel if the firmware returns. */
                for (;;) {
                    __asm__ __volatile__("wfi");
                }
            }
        }
        break;
    case IRQ_U_SOFT:
        cprintf("User software interrupt\n");
        break;
    case IRQ_S_SOFT:
        cprintf("Supervisor software interrupt\n");
        break;
    case IRQ_H_SOFT:
        cprintf("Hypervisor software interrupt\n");
        break;
    case IRQ_M_SOFT:
        cprintf("Machine software interrupt\n");
        break;
    case IRQ_U_TIMER:
        cprintf("User timer interrupt\n");
        break;
    case IRQ_H_TIMER:
        cprintf("Hypervisor timer interrupt\n");
        break;
    case IRQ_M_TIMER:
        cprintf("Machine timer interrupt\n");
        break;
    case IRQ_U_EXT:
        cprintf("User external interrupt\n");
        break;
    case IRQ_S_EXT:
        cprintf("Supervisor external interrupt\n");
        break;
    case IRQ_H_EXT:
        cprintf("Hypervisor external interrupt\n");
        break;
    case IRQ_M_EXT:
        cprintf("Machine external interrupt\n");
        break;
    default:
        cprintf("Unknown interrupt: cause %llu\n", (unsigned long long)cause);
        print_trapframe(tf);
        break;
    }
}

/* *
 * trap_insn_length - length in bytes of the instruction that caused a trap.
 * RISC-V uses the two low bits to distinguish 16-bit compressed instructions
 * (low bits != 0b11) from 32-bit instructions (low bits == 0b11).  ebreak is
 * encoded as the compressed c.ebreak here, so a hard-coded "+ 4" would resume
 * in the middle of the following instruction.
 * */
static uintptr_t trap_insn_length(uintptr_t epc) {
    uint16_t insn = *(const uint16_t *)epc;
    return (insn & 0x3) == 0x3 ? 4 : 2;
}

//lab3 function3 2412855
void exception_handler(struct trapframe *tf) {
    switch (tf->cause) {
    case CAUSE_ILLEGAL_INSTRUCTION:
        cprintf("Illegal instruction at 0x%016lx\n", (unsigned long)tf->epc);
        /* Resume after the illegal instruction instead of re-executing it. */
        tf->epc += trap_insn_length(tf->epc);
        break;

    case CAUSE_BREAKPOINT:
        cprintf("Breakpoint at 0x%016lx\n", (unsigned long)tf->epc);
        /* Skip ebreak (2 or 4 bytes) and return to the next instruction. */
        tf->epc += trap_insn_length(tf->epc);
        break;

    default:
        print_trapframe(tf);
        /*
         * An unexpected exception taken in kernel mode cannot be resumed
         * safely, so report it and stop.  While the kernel still runs without
         * user processes, a frame marked as user mode (SPP == 0) is only
         * reported and then returned to its caller; this also keeps the
         * exception fallback path testable.
         */
        if (trap_in_kernel(tf)) {
            panic("Unexpected exception in kernel, cause %d", (int)tf->cause);
        }
        break;
    }
}



/* *
 * trap - handles or dispatches an exception/interrupt. if and when trap()
 * returns,
 * the code in kern/trap/trapentry.S restores the old CPU state saved in the
 * trapframe and then uses the iret instruction to return from the exception.
 * */
void trap(struct trapframe *tf)
{
    // dispatch based on what type of trap occurred
    if ((intptr_t)tf->cause < 0)
    {
        // interrupts
        interrupt_handler(tf);
    }
    else
    {
        // exceptions
        exception_handler(tf);
    }
}
