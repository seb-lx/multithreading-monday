# operating systems notes related to concurrency
based on:
- Operating Systems: Three Easy Pieces by Andrea Arpaci-Dusseau and Remzi Arpaci-Dusseau

## basics
- the OS acts as an interface between software and hardware
- the OS abstracts from physical resources (processor, memory, disk) through virtualization for ease of use
- "virtualization allows many program to run (thus sharing the CPU), and many programs to concurrently access their own instructions and data (thus sharing memory), and many programs to acces devices (thus sharing disks)"
    - the OS is therefore a resource manager
- virtualization of the CPU by providing the illusion of a seemingly infinite number of virtual CPUs and thus allowing many programs to run concurrently
    - i.e. managing many processes (programs in execution) and their CPU time via context switches, even in the context single-core CPUs used to hide I/O latency through rapid context switching (multiprogramming), multi-core CPUs additionally increase throughput through true parallel execution  
- virtualization of memory by proividing a private, virtual address space for each process (which is seemingly only limited by the amount of RAM available) that the OS maps to physical memory, isolation of individual processes essential for protection
- concurrency is therefore an inherent aspect of the OS (many processes, context switches, interrupts, potentially multiple cores, ...)
- persistent information (disk files) are shared among processes and managed by the file system, system calls provide access
- different privilige levels (user mode, kernel mode) restrict hardware access

## processes
- abstraction: process == running program
- virtualization: time sharing of CPU by running multiple processes at the same time
- a program is executed by loading binary instructions and static data from disk into memory via its own private, virtual address space and then scheduled for execution on the CPU (states: running, blocked, ready)
- a process has therefore an associated machine state: address space (including text/code, data, stack, heap), registers including program counter (PC), stack pointer and frame pointer for managing the stack, open files
    - 
- low-level mechanism of context swtich
- high-level scheduling to decide which process runs next