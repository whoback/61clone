#include "kernel.hh"
#include "k-apic.hh"
#include "k-vmiter.hh"
#include <atomic>

// kernel.cc
//
//    This is the kernel.

// INITIAL PHYSICAL MEMORY LAYOUT
//
//  +-------------- Base Memory --------------+
//  v                                         v
// +-----+--------------------+----------------+--------------------+---------/
// |     | Kernel      Kernel |       :    I/O | App 1        App 1 | App 2
// |     | Code + Data  Stack |  ...  : Memory | Code + Data  Stack | Code ...
// +-----+--------------------+----------------+--------------------+---------/
// 0  0x40000              0x80000 0xA0000 0x100000             0x140000
//                                             ^
//                                             | \___ PROC_SIZE ___/
//                                      PROC_START_ADDR

#define PROC_SIZE 0x40000 // initial state only

proc ptable[NPROC]; // array of process descriptors
                    // Note that `ptable[0]` is never used.
proc *current;      // pointer to currently executing proc

#define HZ 100                           // timer interrupt frequency (interrupts/sec)
static std::atomic<unsigned long> ticks; // # timer interrupts so far

// Memory state
//    Information about physical page with address `pa` is stored in
//    `pages[pa / PAGESIZE]`. In the handout code, each `pages` entry
//    holds an `refcount` member, which is 0 for free pages.
//    You can change this as you see fit.

pageinfo pages[NPAGES];

[[noreturn]] void schedule();
[[noreturn]] void run(proc *p);
void exception(regstate *regs);
uintptr_t syscall(regstate *regs);
void memshow();

// kernel(command)
//    Initialize the hardware and processes and start running. The `command`
//    string is an optional string passed from the boot loader.

static void process_setup(pid_t pid, const char *program_name);

void kernel(const char *command)
{
    // initialize hardware
    init_hardware();
    log_printf("Starting WeensyOS\n");

    ticks = 1;
    init_timer(HZ);

    // clear screen
    console_clear();

    // (re-)initialize kernel page table
    for (vmiter it(kernel_pagetable); it.va() < MEMSIZE_VIRTUAL; it += PAGESIZE)
    {
        if (it.va() >= PROC_START_ADDR || it.va() == CONSOLE_ADDR)
        {
            it.map(it.va(), PTE_P | PTE_W | PTE_U);
        }
        else if (it.va() != 0)
        {
            it.map(it.va(), PTE_P | PTE_W);
        }
        else
        {
            // nullptr is inaccessible even to the kernel
            it.map(it.va(), 0);
        }
    }

    // set up process descriptors
    for (pid_t i = 0; i < NPROC; i++)
    {
        ptable[i].pid = i;
        ptable[i].state = P_FREE;
    }
    if (command && program_loader(command).present())
    {
        process_setup(1, command);
    }
    else
    {
        process_setup(1, "allocator");
        process_setup(2, "allocator2");
        process_setup(3, "allocator3");
        process_setup(4, "allocator4");
    }

    // Switch to the first process using run()
    run(&ptable[1]);
}

// kalloc(sz)
//    Kernel memory allocator. Allocates `sz` contiguous bytes and
//    returns a pointer to the allocated memory, or `nullptr` on failure.
//
//    The returned memory is initialized to 0xCC, which corresponds to
//    the x86 instruction `int3` (this may help you debug). You'll
//    probably want to reset it to something more useful.
//
//    On WeensyOS, `kalloc` is a page-based allocator: if `sz > PAGESIZE`
//    the allocation fails; if `sz < PAGESIZE` it allocates a whole page
//    anyway.
//
//    The handout code returns the next allocatable free page it can find.
//    It never reuses pages or supports freeing memory (you'll change that).

static uintptr_t next_alloc_pa;

void *kalloc(size_t sz)
{
    if (sz > PAGESIZE)
    {
        return nullptr;
    }

    while (next_alloc_pa < MEMSIZE_PHYSICAL)
    {
        uintptr_t pa = next_alloc_pa;
        next_alloc_pa += PAGESIZE;

        if (allocatable_physical_address(pa) && !pages[pa / PAGESIZE].used())
        {
            pages[pa / PAGESIZE].refcount = 1;
            memset((void *)pa, 0xCC, PAGESIZE);
            return (void *)pa;
        }
    }
    return nullptr;
}

// kfree(kptr)
//    Free `kptr`, which must have been previously returned by `kalloc`.
//    If `kptr == nullptr` does nothing.

void kfree(void *kptr)
{
    // assert(pages[(uintptr_t)kptr / PAGESIZE].refcount == 1);
    if (kptr == nullptr)
    {
        log_printf("kptr: %p with refcount=%i match nullptr", kptr, pages[(uintptr_t)kptr/PAGESIZE].refcount);

        return;
    }
    if (allocatable_physical_address((uintptr_t)kptr) && pages[(uintptr_t)kptr / PAGESIZE].refcount != 0)
    {
        log_printf("Free %p with refcount=%i", kptr, pages[(uintptr_t)kptr/PAGESIZE].refcount);

        pages[(uintptr_t)kptr / PAGESIZE].refcount = 0;
    }
    else
    {
        log_printf("Did not free %p with refcount=%i", kptr, pages[(uintptr_t)kptr/PAGESIZE].refcount);
        return;
    }
}

// process_setup(pid, program_name)
//    Load application program `program_name` as process number `pid`.
//    This loads the application's code and data into memory, sets its
//    %rip and %rsp, gives it a stack page, and marks it as runnable.

void process_setup(pid_t pid, const char *program_name)
{
    log_printf("in process setup for %s, pid: %i\n", program_name, pid);
    init_process(&ptable[pid], 0);
   // set_pagetable(kernel_pagetable);

    uintptr_t first_addr = PROC_START_ADDR + (pid - 1) * PROC_SIZE;
    uintptr_t last_addr = PROC_START_ADDR + pid * PROC_SIZE;
    x86_64_pagetable *pt = (x86_64_pagetable *)kalloc(PAGESIZE);
    if(!pt)
    {
        log_printf("Couldn't kalloc pagetable in process_setup");
        kfree((void*)pt);
    }
    memset(pt, 0, PAGESIZE);
    
    ptable[pid].pagetable = pt;
    for (vmiter ita(kernel_pagetable, 0), itb(ptable[pid].pagetable, 0);
         ita.va() < PROC_START_ADDR;
         ita += PAGESIZE, itb += PAGESIZE)
    {
        
            itb.map(ita.pa(), ita.perm());
        
    }

    // load the program
    program_loader loader(program_name);

    // allocate and map all memory
    for (loader.reset(); loader.present(); ++loader)
    {
        for (uintptr_t a = round_down(loader.va(), PAGESIZE);
             a < loader.va() + loader.size();
             a += PAGESIZE)
        {
            assert(a >= first_addr && a < last_addr);
            assert(!pages[a / PAGESIZE].used());
            void *p = kalloc(PAGESIZE);
            if (!p)
            {
                kfree(p);
                log_printf("couldn't kalloc in loader\n");
            }
            memset(p, 0, PAGESIZE);
            memcpy(p, loader.data(), loader.data_size());
            int t = vmiter(ptable[pid].pagetable, a).try_map((uintptr_t)p, PTE_P | PTE_W | PTE_U);
            if(t!=0)
            {
                log_printf("Failed mapping in loader");
            }
        }
    }

    // console_printf("Out of loader.\n");
    // copy instructions and data into place
    set_pagetable(ptable[pid].pagetable);
    for (loader.reset(); loader.present(); ++loader)
    {
        memset((void *)loader.va(), 0, loader.size());
        memcpy((void *)loader.va(), loader.data(), loader.data_size());
    }

    set_pagetable(kernel_pagetable);
    // mark entry point
    ptable[pid].regs.reg_rip = loader.entry();

    // allocate stack
   // uintptr_t stack_addr = PROC_START_ADDR + PROC_SIZE * pid - PAGESIZE;
    uintptr_t stack_addr = MEMSIZE_VIRTUAL - PAGESIZE;
    assert(!pages[stack_addr / PAGESIZE].used());
     //pages[stack_addr / PAGESIZE].refcount = 1;
    void *p = kalloc(PAGESIZE);
    if (!p)
    {
        kfree(p);
        log_printf("No memory for stack allocation");
    }
    memset(p, 0, PAGESIZE);
    int t = vmiter(ptable[pid].pagetable, stack_addr).try_map((uintptr_t)p, PTE_P | PTE_W | PTE_U);
    if (t != 0)
    {
        log_printf("failed stack map");
    }
    ptable[pid].regs.reg_rsp = stack_addr + PAGESIZE;
    // mark process as runnable
    ptable[pid].state = P_RUNNABLE;
    // console_printf("Stack");
}

// exception(regs)
//    Exception handler (for interrupts, traps, and faults).
//
//    The register values from exception time are stored in `regs`.
//    The processor responds to an exception by saving application state on
//    the kernel's stack, then jumping to kernel assembly code (in
//    k-exception.S). That code saves more registers on the kernel's stack,
//    then calls exception().
//
//    Note that hardware interrupts are disabled when the kernel is running.

void exception(regstate *regs)
{
    // Copy the saved registers into the `current` process descriptor.
    current->regs = *regs;
    regs = &current->regs;

    // It can be useful to log events using `log_printf`.
    // Events logged this way are stored in the host's `log.txt` file.
    /* log_printf("proc %d: exception %d at rip %p\n",
                current->pid, regs->reg_intno, regs->reg_rip); */

    // Show the current cursor location and memory state
    // (unless this is a kernel fault).
    console_show_cursor(cursorpos);
    if (regs->reg_intno != INT_PF || (regs->reg_errcode & PFERR_USER))
    {
        memshow();
    }

    // If Control-C was typed, exit the virtual machine.
    check_keyboard();

    // Actually handle the exception.
    switch (regs->reg_intno)
    {

    case INT_IRQ + IRQ_TIMER:
        ++ticks;
        lapicstate::get().ack();
        schedule();
        break; /* will not be reached */

    case INT_PF:
    {
        // Analyze faulting address and access type.
        uintptr_t addr = rdcr2();
        const char *operation = regs->reg_errcode & PFERR_WRITE
                                    ? "write"
                                    : "read";
        const char *problem = regs->reg_errcode & PFERR_PRESENT
                                  ? "protection problem"
                                  : "missing page";

        if (!(regs->reg_errcode & PFERR_USER))
        {
            panic("Kernel page fault for %p (%s %s, rip=%p)!\n",
                  addr, operation, problem, regs->reg_rip);
        }
        console_printf(CPOS(24, 0), 0x0C00,
                       "Process %d page fault for %p (%s %s, rip=%p)!\n",
                       current->pid, addr, operation, problem, regs->reg_rip);
        current->state = P_BROKEN;
        break;
    }

    default:
        panic("Unexpected exception proc %d: exception %d at rip %p\n",
                current->pid, regs->reg_intno, regs->reg_rip);
    }

    // Return to the current process (or run something else).
    if (current->state == P_RUNNABLE)
    {
        run(current);
    }
    else
    {
        schedule();
    }
}

// syscall(regs)
//    System call handler.
//
//    The register values from system call time are stored in `regs`.
//    The return value, if any, is returned to the user process in `%rax`.
//
//    Note that hardware interrupts are disabled when the kernel is running.

int syscall_page_alloc(uintptr_t addr);
pid_t sys_fork(void);
uintptr_t syscall(regstate *regs)
{
    // Copy the saved registers into the `current` process descriptor.
    current->regs = *regs;
    regs = &current->regs;

    // It can be useful to log events using `log_printf`.
    // Events logged this way are stored in the host's `log.txt` file.
    /* log_printf("proc %d: syscall %d at rip %p\n",
                  current->pid, regs->reg_rax, regs->reg_rip); */

    // Show the current cursor location and memory state
    // (unless this is a kernel fault).
    console_show_cursor(cursorpos);
    memshow();

    // If Control-C was typed, exit the virtual machine.
    check_keyboard();

    // Actually handle the exception.
    switch (regs->reg_rax)
    {

    case SYSCALL_PANIC:
        panic(nullptr); // does not return

    case SYSCALL_GETPID:
        return current->pid;

    case SYSCALL_YIELD:
        current->regs.reg_rax = 0;
        schedule(); // does not return

    case SYSCALL_PAGE_ALLOC:
    {

        return syscall_page_alloc(current->regs.reg_rdi);
    }
    case SYSCALL_FORK:
    {
        return sys_fork();
    }
    default:
        panic("Unexpected system call %ld!\n", regs->reg_rax);
    }

    panic("Should not get here!\n");
}

pid_t sys_fork(void)
{

    pid_t pid_fork = -1;

    // find a free slot in ptable for new process
    for (pid_t i = 1; i < NPROC; ++i)
    {
        if (ptable[i].state == P_FREE)
        {
            // create new process pagetable
            x86_64_pagetable *pagetable_fork = (x86_64_pagetable *)kalloc(PAGESIZE);
            if (!pagetable_fork)
            {
                kfree(pagetable_fork);
                return -1;
            }
            memset(pagetable_fork, 0, PAGESIZE);
            // add new pagetable to ptable
            ptable[i].pagetable = pagetable_fork;
            // set process state as non free
            ptable[i].state = P_RUNNABLE;
            pid_fork = i;

            // copy the mappings from kernel_pagetable into new page table
            for (vmiter ita(current->pagetable, 0), itb(ptable[i].pagetable, 0);
                 ita.va() < MEMSIZE_VIRTUAL; ita += PAGESIZE, itb += PAGESIZE)
            {

                // check if parent process has an application-writable page at virtual address V,
                // then fork must allocate a new physical page P
                if (ita.writable() || ita.user() || ita.va() == CONSOLE_ADDR)
                {
                    // allocate new physical page for new pagetable
                    uintptr_t p = (uintptr_t)kalloc(PAGESIZE);
                    if (!(void *)p)
                    {
                        kfree((void *)p);
                        return -1;
                    }
                    memset((void *)p, 0, PAGESIZE);
                    // set va in new page table to 0
                    memset((void *)itb.pa(), 0, PAGESIZE);

                    // copy the data from the parent’s page into P, using memcpy
                    memcpy((void *)p, (void *)ita.pa(), PAGESIZE);

                    // map page P at address V in the child process’s page table.
                    itb.map(p, ita.perm());
                }
                else
                {
                    itb.map(ita.pa(), ita.perm());
                }
            }

            // return process id
            ptable[i].regs = current->regs;
            ptable[i].regs.reg_rax = 0;
            
        }
    }
    // return -1 if no slot in ptable for process
    if (pid_fork == -1)
    {
        log_printf("!!! returned -1 from fork !!!! ");
        return -1;
    }
    log_printf("@@@ returned pid %d @@@", pid_fork);
            return pid_fork;
}
// syscall_page_alloc(addr)
//    Handles the SYSCALL_PAGE_ALLOC system call. This function
//    should implement the specification for `sys_page_alloc`
//    in `u-lib.hh` (but in the handout code, it does not).

int syscall_page_alloc(uintptr_t addr)
{
    //console_printf("It got to syscall_page_alloc");
    if ((addr % PAGESIZE != 0) || (addr < PROC_START_ADDR) || (addr >= MEMSIZE_VIRTUAL))
    {
        log_printf("bad address at: %u\n", addr);

        return -1;
    }

    if (vmiter(current, addr).user() || pages[addr / PAGESIZE].used())
    {
        kfree((void *)vmiter(current, addr).pa());
    }

    void *p = kalloc(PAGESIZE);
    if (!p)
    {
        log_printf("No phys memory for sycall\n");
        kfree(p);
        return -1;
    }
    memset((void *)p, 0, PAGESIZE);
    memcpy((void *)addr, p, PAGESIZE);

    // console_printf("in sycall\n");
    pid_t pid = current->pid;
    int t = vmiter(ptable[pid].pagetable, addr).try_map((uintptr_t)p, PTE_P | PTE_W | PTE_U);
    if (t != 0)
    {
        log_printf("error syscall alloctrymap");

        return -1;
    }
    return 0;
}

// schedule
//    Pick the next process to run and then run it.
//    If there are no runnable processes, spins forever.

void schedule()
{
    pid_t pid = current->pid;
    for (unsigned spins = 1; true; ++spins)
    {
        pid = (pid + 1) % NPROC;
        if (ptable[pid].state == P_RUNNABLE)
        {
            run(&ptable[pid]);
        }

        // If Control-C was typed, exit the virtual machine.
        check_keyboard();

        // If spinning forever, show the memviewer.
        if (spins % (1 << 12) == 0)
        {
            memshow();
            log_printf("%u\n", spins);
        }
    }
}

// run(p)
//    Run process `p`. This involves setting `current = p` and calling
//    `exception_return` to restore its page table and registers.

void run(proc *p)
{
    assert(p->state == P_RUNNABLE);
    current = p;

    // Check the process's current pagetable.
    check_pagetable(p->pagetable);

    // This function is defined in k-exception.S. It restores the process's
    // registers then jumps back to user mode.
    exception_return(p);

    // should never get here
    while (true)
    {
    }
}

// memshow()
//    Draw a picture of memory (physical and virtual) on the CGA console.
//    Switches to a new process's virtual memory map every 0.25 sec.
//    Uses `console_memviewer()`, a function defined in `k-memviewer.cc`.

void memshow()
{
    static unsigned last_ticks = 0;
    static int showing = 0;

    // switch to a new process every 0.25 sec
    if (last_ticks == 0 || ticks - last_ticks >= HZ / 2)
    {
        last_ticks = ticks;
        showing = (showing + 1) % NPROC;
    }

    proc *p = nullptr;
    for (int search = 0; !p && search < NPROC; ++search)
    {
        if (ptable[showing].state != P_FREE && ptable[showing].pagetable)
        {
            p = &ptable[showing];
        }
        else
        {
            showing = (showing + 1) % NPROC;
        }
    }

    extern void console_memviewer(proc * vmp);
    console_memviewer(p);
}