# concurrency notes
based on:
- C++ Concurrency in Action Second Edition by Anthony Williams
- Multithreading in C++ Lecture at TU Darmstadt

## why parallel computing?
- it is not possible to create faster uniprocessors forever (Moore's Law will come to an end, power and heat limit frequency, memory performance bottleneck, instruction-level parallelism diminishing returns compared to large performance boosts in the past through e.g. pipelining and branch prediction)
- multi-core systems place multiple processors on the same chip that share resources, many-core systems (such as GPU's) utilize huge number of cores
- thread-level parallelism as the only option to significantly speed up programs

## concurrency vs. parallelism
- often overlapping meaning but concurrency as a more broader concept 
- concurrency in computer systems means performing multiple separate activities at the same time rather than sequentially
    - does not require parallel hardware: in uniprocessor systems the OS also runs multiple applications at the same time giving the illusion of parallelism through fast task switching
    - concurrency generally refers to separation of concerns (grouping related code as conceptional design and independent of CPU cores) and responsiveness
- parallelism (true parallelism, true concurrency, genuine concurrency)
    - does require parallel hardware: in multi-core systems multiple activities are run in parallel on separate processing units
    - parallelism generally refers to taking advantage of parallel hardware to increase performance of bulk data processing
- using concurrency for performance by
    - dividing a task into subtasks to run in parallel like a pipeline, each thread performs a different well-defined function/task (functional/task parallelism) -> does not scale very well
    - dividing data into parts to be independently processed, each thread performs same function on different parts of the data (data parallelism)  -> scales well with amount of data, especially embarassingly parallel algorithms

## parallel computing architecture
classification based on number of concurrent insrtuction streams and data streams (Flynn's Taxonomy)
- single instruction single data (SISD): classical uniprocessors
- singe instruction multiple data (SIMD): data parallelism, single instruction multiple threads (SIMT) used in GPU's
- multiple instructions multiple data (MIMD): general purpose multi-processors, multi-core CPUs
    - operating on shared memory
        - uniform memory access (UMA): each core has exactly the same access time to RAM
        - non-uniform memory access (NUMA): RAM is divided and distributed across different physical memory controllers, forming clusters/NUMA nodes with dedicated RAM slots, access time of individual cores to memory differs, used in modern multi-core precessors
    - operating on distributed memory

- modern multi-core systems use MIMD and NUMA architecture (although individual cores also have SIMD units that can be exploited in modern C++ programs e.g. for vector instructions and other optimizations)
    - MIMD layer: std::thread and std::jthread to launch threads on different cores
    - SIMD layer: specialized units on individual cores for vectorization
- in summary, modern CPUs combine multiple forms of parallelism:
    - thread-level parallelism (TLP): multiple physical cores on a single chip, utilized effectively by multi-threading, MIMD
    - instruction-level parallelism (ILP): each core executes multiple independent instructions simultaneously (pipelining), branch prediction, ...
    - data-level parallelism (DLP): each instruction is potentially executed on vectors (single on multiple data elements at once), vectorization, SIMD
        - a core contains scalar execution units and vector execution units, vector registers and vector arithmetic instructions to process multiple values at the same time (e.g. AVX, AVX-512, SSE, ...)
        - improvement in the throughput department rather than latency, especially useful for bulk data (arrays) that is contiguous in memory with predictable access patterns 
        - in C++ compilers can autovectorize loops when applicable (no dependencies, ...) and compile to SIMD vector instructions dependent on target architecture and instruction set, need optimizations -O3 and -march=native
        - can also use intrinsic SIMD instructions explicitly (which limits portability)

- memory management and optimizations for high-performance
    - cache optimization has the goal to minimize RAM access through data layout and access patterns (e.g. cache-friendly data structures have contigous memory layout so that multiple data elements fit into same cache lines)
    - if RAM needs to be accessed, NUMA optimization has the goal to optimize thread affinity to cores and memory allocation policies
        - there are options to pin a spawned std::thread to specific physical cores to improve its affinity to the accessed data, the OS will not bounce the thread across different NUMA nodes
        - in general memory is allocated virtually which means that physical memory is only allocated when a thread writes to/touches it, the OS then allocates the RAM closest to the accessing core, there are options to optimize for this by using the same threads to initialize the memory that they are later processing (or specifically use NUMA libraries to allocate memory on a NUMA node)


## parallel performance
- speedup measures improvement in execution time: S = (execution time without improvement) / (execution time with improvement)
- Amdahl's law can be used to find the maximum theoretical speedup for a given task: S = 1 / (f_seq + ((1 - f_seq) / p)) where f_seq is the part that is sequential, p are the number of processors, and thus (1 - f_seq) is the part that can be parallelized 
    - only some part of the task can be parallelized, so the overall improvement is limited by the serial part, optimization should therefore focus on reducing the serial part: S < (1 / f_seq) with ever increasing p
    - applies only to computing a given problem faster, ignoring problem size


## complexity
- asymptotic complexity theory is used to reason about the scalability of algorithms (with respect to time or space)
- Assume an algorithm or function T that represents the performed work, then the algorithm or function depends on the input size: T(n)
    - in the context of concurrency, it can also depend on the number of processors: T(n, p)
- Big O notation denotes an upper bound for algorithm or function T(n), worst case
    - T(n) is upper bounded by O(f(n)) iff there exist positive constants c and n_0 with T(n) <= c * f(n) for all n >= n_0
    - multiple upper bounds exist, find f(n) that is most tight upper bound
    - then with e.g. f(n) = n, O(f(n)) = O(n)
- Big Omega notation denotes a lower bound for algorithm or function T(n), best case
    - T(n) is lower bounded by Ω(f(n)) iff there exist positive constants c and n_0 with T(n) >= c * f(n) for all n >= n_0
    - multiple lower bounds exist, find f(n) that is most tight lower bound
    - then with e.g. f(n) = nlog(n), Ω(f(n)) = Ω(nlog(n))
- Big Theta notation denotes an upper and lower bound (exact bound) for algorithm or function T(n), average case
    - T(n) is upper and lower bounded by θ(f(n)) iff there exist positive constants c_1, c_2, and n_0 with c_1 * f(n) <= T(n) <= c_2 * f(n) for all n >= n_0
    - then with e.g. f(n) = n, θ(f(n)) = Ω(n)

- Example: compute dot product of two vectors in parallel
    - split vectors into pieces of length n / p, compute subproducts in parallel, compute global sum in tree-like fashion
    - complexity for T(n, p) is θ(n/p + log(p))

## processes and threads
processes and threads are hardware abstractions, "A process is a unit of resources, while a thread is a unit of scheduling and execution."

### processes
- processes are programs in execution
- OS context switches between multiple processes for executing on the CPU
- used to group resources, each has own address space (stack, heap, data, text)


### threads
- threads are basic units of CPU utilization, i.e. smallest sequence of programmed instructions managed independently by a scheduler
- in contrast to processes being the abstraction for grouping resources, the processes threads are scheduled for execution, each process has at least one thread (the thread running main() started by the runtime)
- all threads of a process live in the address space of the process
    - they therefore share heap memory, data section, text/code section, and OS resources
    - however, each thread has its own stack memory (for local variables, function parameters, return addresses, ...), registers, and program counter (PC, points to instruction to be executed next), as well as an associated ID 
    - if the OS context switches between threads the state of the current thread is saved and the state of the new thread loaded, due to the program counter being unique to each thread execution can continue where it left off
    - warning: in theory, in C++ a thread can access the stack memory of other threads via a pointer since they share the same overall address space, so passing pointers to stack variables across threads is undefined behavior (if the stack variable is destroyed, we have a dangling pointer)
- Creating too many threads can cause problems
    - each thread requires separate stack space, the process has only limited memory
    - if the number of created threads surpasses the available cores (oversubscription), context switching is needed again
        - Creating more threads than cores/hardware threads available (resulting in the OS to manage and context switch between them), "... is called concurrency (managing many things at once), whereas having enough cores/hardware threads to run them simultaneousy is parallelism (doing many things at once)"
    - the number of threads should therefore take into account the CPU hardware and thus available hardware threads / CPU cores
        - due to "simultaneous multithreading" (aka. hyperthreading), one physical core is viewed as two hardware threads (i.e. logical cores, the OS view of a core). In this case the number of hardware threads is different than the number of physical cores.
        - C++ allows to check this upper limit for true hardware parallelism using std::thread::hardware_concurrency()

#### user threads vs kernal threads
- user level threads are created in user space and managed by application or library, lightweight and fast, OS kernel is unaware and sees only single process, if one user thread performs blocking I/O the entire process is block and thus all other threads
- kernel level threads are managed by the OS, context switching is more expensive, blocking is not a problem since OS can context switch to another thread
- different types of mappings
- in C++ every std::thread or std::jthread maps 1:1 to kernel threads, in this 1:1 mapping (typical for windows and linux) the OS switches not between processes anymore but rather between threads of processes

```cpp

auto f(const std::string& s) -> void { std::cout << s << "\n"; }


auto managing_threads() -> void {

    // creating threads means associating a std::thread object with a new
    // thread of execution
    //
    // threads execute a specified function and exit when the function returns
    // every thread needs to be either
    //  - joined by waiting for it to finish (a join() cleans up any storage
    //    associated with the threadthe std::thread object is not associated
    //    anymore with the hardware thread, not joinable() afterwards, can
    //    join() threads only once) or
    //  - detached by leaving it running in the background, need to ensure
    //    accessed data is valid until detached thread (daemon) exits (a
    //    detach() can also only be called on threads that are joinable()) 
    //
    // in the presence of exceptions we need to make sure that the thread
    // objects appropriately join() to avoid lifetime issues
    //  - i.e. join() in any path, also in catch block
    //  - best practice is to use a std::jthread which follows RAII idiom
    //    by calling join() in its destructor and thus automatically cleaning
    //    up upon destruction, even in the presence of exceptions

    auto t1 = std::thread(f, "hello");
    t1.join();
    //t1.detach();
    auto t2 = std::jthread(f, "hello");


    // by default arguments are copied into the internal storage of the thread
    // and then passed to the function (or callable object) as rvalues as if
    // they were temporaries
    // 
    // this can be problematic if the detached thread outlives the scope of the 
    // local variables it depends on, i.e. by an implicit conversion of the 
    // passed argument that is yet to be performed even if main already exited
    //  -> therefore use explicit cast when passing arguments to avoid this

    {
        auto buffer = std::array<char, 1024>{};
        sprintf(buffer.data(), "%i", some_param);
        //auto t = std::thread(f, buffer.data()); // passing pointer
        auto t = std::thread(f, std::string(buffer.data())); // fix
        t.detach();
    } // buffer stack variable is destroyed here


    // std::thread objects hold a resource that is unique to them: the
    // associated thread of execution
    //  -> std::thread objets are therefore movable (transfer ownership) but 
    //     not copyable, copying a hardware thread does not make sense,
    //     at any time there is only one std::thread object associated with a
    //     particular thread of execution

    // associate new thread with std::jthread object t3
    //  -> before C++17: "create temporary std::jthread object and then move
    //                    construct into t3"
    //  -> since C++17:  "directly construct std::jthread object inside t3,
    //                    entirely bypass temporary and the move constructor 
    //                    (treat right side as recipe for initialization due to
    //                    mandatory copy elision)"
    auto t3 = std::jthread(f, "hello");

    // explicitly transfer ownership from t3 to t4, t3 has no longer an
    // associated thread of execution
    auto t4 = std::move(t3);

    // associate new thread with temporary std::jthread object 
    //  -> before C++17: "create temporary std::jthread object and then move
    //                    assign into t3, no std::move() needed because moving
    //                    from temporaries is automatic and implicit"
    //  -> since C++17:  "move assign into t3 by directly materializing right 
    //                    side as argument for move assignment operator
    //                    (treat right side as recipe for move assignment due 
    //                    to mandatory copy elision)"
    t3 = std::jthread(f, "hello");

    // default construct t5, no associated thread of execution yet
    auto t5 = std::jthread{};

    // move (transfer ownership) of t4 into t3
    t5 = std::move(t4);

    // WARNING: would drop thread of execution associated with t3 which is not
    //          possible and would terminate()
    //t3 = std::move(t5);


    // choose number of threads based on hardware concurrency which is an
    // indication of how many threads can truly run concurrently, e.g. 
    // available (logical) CPU cores (just a hint, can be zero)
    // 
    // can be used to divide data evenly across available threads (in the case 
    // of uneven division main can process rest)

    const auto hardware_threads = std::thread::hardware_concurrency();


    // each thread has an associated ID of type std::thread::id
}
```



## sharing data between threads
one of the main causes of concurrency-related bugs is the incorrect use of shared data.

- if data is read-only there is never a problem, it is all about modification of shared data
- if one thread writes to shared data, other threads could see broken invariants during update while accessing that same data, leading to invalid read operations
- a race condition happens when the outcome depends on the relative ordering of execution of operations on multiple threads, non-deterministic
    - which can be benign (assume multiple threads adding elements to a queue for further processing and order is irrelevant)
    - problematic race conditions occur mostly when an operation consists of executing multiple instructions, and another thread could access in between those individual instructions
        - assume for example "counter = counter + 1;" where the operation consists of: loading counter from memory into register, incrementing value, storing result into memory. if these instructions are not performed atomically, there can be problems
- a data race is a specific type of race condition which arises when multiple threads can access a memory location simultaneously and at least one of their accesses is a write operation
    - undefined behavior
- avoiding race conditions by using a single thread, using locks for protection of invariants (lock-based programming), leveraging atomic operations and memory ordering to preserve invariants (lock-free programming)
- lock-based programming using a mutex (ensuring mutually exclusive access of data) as synchronization primitive
    - consider the following scenario: two threads access some shared data that is protected by a std::mutex. if thread A acquires the lock and thread B tries to lock the mutex, thread B will block until the mutex is released. thread B will not waste CPU cylces since the OS will change the thread from running/ready to blocked and thus descheduled so that the CPU can do other things. internally the kernel will put the sleeping thread B into a waiting queue. if thread A finishes its work and releases the lock the operating system will be informed via a system call. the OS will then remove the sleeping thread from the queue (specific to mutex), change it to ready, and the scheduler can then normally schedule the thread for execution. if thread B is chosen to execute, it continues inside the lock call and acquires the lock ( assuming no other thread acquired it in the meantime).
        - note: this is similar to condition variables but differs in the way the wakup signal is issued. in the normal mutex lock case, the wakup is implicit when the other thread releases the resource. in the condition variable case the wakup is signaled by another thread within the program logic explicitly. if you would simply use a mutex to wait for a logical condition within the program, you would need to implement a busy-wait within a loop to check the condition constantly while acquiring and releasing the lock which wastes CPU cycles. condition variables are designed to circumvent and solve the busy-wait problem.
    - need to protect right data at right granularity, avoid deadlocks, ...

- a deadlock is a situation where two or more threads are waiting indefinitely for an event to occur that can only be caused by one of the waiting threads
    - for example one threads locks mutex1 and then mutex2, the other thread locks mutex2 and then mutex1
    - solutions: use std::lock and std::adopt_lock (or simply std::scoped_lock since C++17) to lock multiple mutexes at the same time
    - if one needs to acquire locks separately: avoid nested locks so that it is avoided to acquire a lock when already holding one, acqurie locks in fixed order, use lock hierarchy to enfore fixed order, use backoff strategy with try_lock,  do not call user code when holding a lock
    - generally also join() threads in the same function that started them
- locking can be deferred with std::unique_lock and std::lock
- lock granularity determines the amount of data protected by the lock
    - performance tradeoff (coarse granularity protects more data / instructions hindering concurrency, finer granularity )
    - in general holding locks longer than required can cause a drop in performance because other threads waiting for the lock are prevented to acquire it as long as it is not released, therefore the more fine grained the lock the better (assuming correctness and required data is protected sufficiently)
    - lock mutex only as long as shared data is accessed
    - do not lock mutexes while doing time consuming I/O
    - one could start from coarser granularity and move to finer granularity

```cpp
// best to group mutex and data together
class data_wrapper {
private:
    std::mutex m_;
    int data_;
};


auto sharing_data_between_threads() -> void {

    // since C++17 use std::scoped_lock instead of std::lock_guard
    // as a strict upgrade. it can lock multiple mutexes without deadlock.
    // this fixes the problem of manual locking multiple mutexes, when not done
    // in the same order can lead to a deadlock

    // !!!
    // warning: it must be avoided to pass references or pointers to data
    //          outside of the scope of the lock
    //          any other piece of code that has access to the pointer or
    //          reference can modify the data

    // race conditions can be inherent in interfaces, e.g. for a stack
    // having separate methods for top() and pop() can cause issues
    // combination in one method needed
}
```


## synchronization of concurrent operations
sometimes a thread needs to wait for an operation to finish it would be undesirable for the thread to waste CPU cycles waiting if it couzld do other work in the meantime

- condition variables can be used to wait for a specific event that other threads can set / notify others
    - they are basically an optimization for busy-waits (looping until an event happens and wasting computing resources - thread is still scheduled and instructions normally executed), waiting for condition variables puts the thread to sleep and the OS scheduler will not schedule it until it is set to ready when the condition variable is notified
    - spurious wakes can happen even if no thread notified
    - useful for continous waiting and processing, if waited only once on condition variable, use futures instead
- latches to wait for event that happens once after count down permanently
- barriers to wait for event that happens once after count down
- futures to wait for event that happens once

```cpp
auto synchronization_of_concurrent_operations() -> void {

    // futures can be computations in background that return a value
    // multiple ways to associate a future with a task
    //  1. std::async
    //  2. std::packaged_task
    //  3. std::promise

    // std::async either runs on own thread or is deferred until get() call
    std::future<int> answer = std::async(find_answer);
    // ...
    auto answer_result = answer.get(); // wait for result to be ready

    // std::packaged_task to tie a std::future to a function or task
    // and the packaged_task can be passed around. abstracts from the specific
    // task
    //  - represents a task together with instructions on how to solve it
    std::packaged_task<int()> task(find_answer);
    std::future<int> task_future = task.get_future();
    // ...
    auto answer_result = task_future.get();

    // std::promise can be used if tasks cannot be expressed as single function
    // calls, result can be computed from multiple places, simply set result
    // value that can be later retrieved with a future. this is basically the 
    // lowest abstraction
    //  - represents a task without instructions on how to solve it
    std::promise<int> p{}; // inactive
    std::future<int> f = p.get_future(); // activate promise by getting future
    // ...
    p.set_value(42); // satisfy promise, or set exception
    f.get();

    // If exceptions are thrown, the exception is stored inside the std::future
    // get() rethrows in that case. if std::promise or std::packaged_task is
    // destroyed before setting result, an exception is stored

    // std::future is movable but not copyable, only one thread can wait
    // std::shared_future is copyable, multiple threads can wait

    // can also wait for specific time through f.wait_for()

    // std::future enables functional programming style where each task
    // produces a result entirely dependent on its input rather than the 
    // external environment, no explicit access to shared data
}
```

## memory model
C++ has a multi-threading aware memory model. atomic types and operations provide low-level synchronization commonly reduced to a one or two CPU instructions.


### structural aspect (how things are laid out in memory)

An object in C++ is simply a region in memory that has an associated type, size, and lifetime. By this definition all variables are objects.
- in contrast, a pure object-oriented language where "everything is an object" typically refers to everything being an instance of a universal root "Object" class. this results in fundamental types being a class with member functions or deriving from fundamental types like int. fundamental types have therefore an overhead because of metadata such as method tables associated with the type.
- in C++ fundamental types are built into the compiler and are not defined in software libraries, fundamental types are raw data not class instances

every object occupies at least one memory location. variables of fundamental types occupy exactly one memory location

### concurrency aspect
if multiple threads access separate memory locations, there are no problems. if they access the same memory location in a read-only fashion, there are no problems. if they access the same memory location and at least one thread modifies the data, there is a potential race condition.

in order to avoid the race condition there has to be an enforced ordering between each pair of accesses (either a fixed ordering or an ordering that varies between runs but guaranteeing some ordering). this is achieved either through the use of a mutex or the synchronization properties of atomic operations.

"If there’s no enforced ordering between two accesses to a single memory location from separate threads, one or both of those accesses is not atomic, and if one or both is a write, then this is a data race and causes undefined behavior."

atomic operations are used to replace mutex based synchronization (sometimes atomic types are implemented using mutexes under the hood, so it should be checked if that is the case because performance depends on that)
- check via std::atomic<T>::is_always_lock_free, built-in types should be lock-free on most systems but it is not required
- std::atomic_flag is a simple boolean flag, required to be lock-free
- all atomic types cannot be copy-constructed or copy-assigned because these operations include two different objects (read from one, written to the other). a single operation on two distinct objects cannot be atomic, and therefore are not permitted.

### modification orders
this applies only to data that is shared between threads (no thread-local or stack data)

- enforced modification orders through mutexes or atomics ensure that each thread sees a coherent view of memory. a modification order is a single, independent, global timeline of values for one specific variable starting at its initialization. threads writing to that variable append the value at the end of the modification order. the order will vary between runs, but in any given execution all threads in the system must agree on the order (because "The cache coherence protocol (e.g. MESI on x86) is what enforces agreement between cores running truly in parallel. The cores negotiate over the cache bus/interconnect. One store wins the bus arbitration and becomes visible first — that's what establishes the order. ... The threads run in parallel; the visibility of their writes gets serialized by the hardware."). there are several rules that prevent the CPU from applying aggressive reorder optimizations that would break this coherent view such as speculative execution.

- "The C++ Abstract Machine: The C++ memory model is defined abstractly. It states that if two expressions in different threads evaluate the same memory location concurrently, and at least one modifies it, they must be synchronized. If they are not, it is a data race. The standard does not care if you have 1 core or 1,000 cores; un-synchronized interleaved execution shatters the logical modification order all the same."

- happens-before and synchronize-with relationships
    - synchronize-with relationship only possible for operations between atomic types, suitably-tagged atomic write synchronizes with suitably-tagged atomic read, enables inter-thread happens-before relationships
    - happens-before relationship specifies which operations see the effects of which other operations
        - in the single-threaded case (or within one thread, intra thread) this is simple as each operation that sequenced before another it happens before it
        - in the multi-threaded case, if operation A in one thread synchronizes with operation B in another thread, then A inter-thread happens before B
        - it is also transitive

- there are three models of ordering which influence the consequences of operation ordering and synchronize-with relationship
    - sequentially consistent ordering (std::memory_order_seq_cst) is the default: behavior of the program is consistent with sequential view, implies total ordering, "if all operations on instances of atomics types are sequentially consistent, the behavior of a multithreaded program is as if all these operations were performed in some particular sequence by a single thread.", can however have a large performance penalty since synchronization between processors/cores is required for a consistent view
    - non-sequentially consistent orderings: there is no single global order of events that every thread agrees to and sees, there are no more neatly interleaving operations, true concurrent execution, different CPU caches and internal buffers can hold different values for the same memory, compiler instruction reordering
        - relaxed ordering (std::memory_order_relaxed): atomic types with relaxed ordering do not participate in synchronizes-with relationship, no requirement on ordering relative to other threads
        - acquire-release ordering (std::memory_order_consume, std::memory_order_acquire, std::memory_order_release, and std::memory_order_acq_rel): atomic loads are acquire operations, atomic stores are release operations, and atomic read-modify-write operations are either or both. synchronization is pairwise between threads, release synchronizes-with acquire that reads the written value, "In order to provide any synchronization, acquire and release operations must be paired up. The value stored by a release operation must be seen by an acquire operation for either to have any effect.

- within a thread the operations on atomic variables are always sequenced-before (happens-before relationship) regardless of memory order. the memory order only changes the inter-thread visibility guarantees

- in addition of thinking about thread interleaving, for atomics think about "when a thread reads a value, which write does it actually see?"


seq_cst total order
│
│   "All seq_cst operations have a universal timestamp.
│    Everyone agrees on the order, period."
│
└──▶ synchronized-with (subset of the above)
        │
        │   "This specific load read this specific store's
        │    value — so everything before the store
        │    happens-before everything after the load."
        │
        └──▶ happens-before chain




- using atomic types for variables will ensure that the compiler generates necessary synchronization instructions (barriers/fences) so that physical cache controllers enforce this strict global modification order across all CPU cores automatically. for example if a thread reads an atomic integer and increments it (fetch and add), the core it runs on locks the cache line for that variable. even if another thread on another core accesses and increments the same variable, it must wait until the first core is done and is thus forced to read the updated value. using atomic variables forces the compiler to generate a single, unbroken, globally agreed-upon timeline.

- Question: "Why are we talking about a "timeline" of variable values, instead of a coherent current value of a variable?"
    - mainly due to multi-core hardware caches. each core caches values, if it is a shared value, each cache might contain a different value before all caches are synchronized, and since there is a propagation delay the modification order would collapse if it is not enforced through cache coherence

Core 1               Core 2
  |                    |
Store Buffer        Store Buffer
  |                    |
L1 Cache  ←——→  L1 Cache   (cache coherence protocol)
  |                    |
        L2/L3 Cache (shared)
              |
            RAM

- using mutexes or atomics basically introduce memory fences.
    - mutex lock() and unlock() mark the critical section and tell the CPU that no instructions within that critical section is allowed to be either speculatively executed before or delayed past it. the CPU can change the order of instructions within and outside of the critical section (the memory fences). it must make sure that no instructions leak outside.
    - by default atomic operations use sequential consistency memory ordering (std::memory_order_seq_cst) that introduces memory fences when using atomic instructions. these memory fences regulate only memory operations (RAM/cache loads/reads and stores/writes), all other instructions (like local registers) are independent and can be optimized by the CPU. in the case of atomic store() operations: all memory operations must be executed before (memory writes are buffered and later applied in batch, this ensures that every single memory write is flushed), it acts as physical gate that prevents the CPU from reordering so that a memory write before the atomic store in the code is also guaranteed to be executed before
        - "A default atomic write, however, forces the core to empty that Store Buffer, lock the cache line, and synchronize across the hardware interconnect with all other cores." -> can relax rules
            - relaxed: basically only ensures that atomic instructions are performed atomically without any constraints on memory ordering

- SO BASICALLY IN THE DEFAULT SEQUENTIAL CONSISTENT MEMORY ORDERING AN ATOMIC STORE MAKES SURE ALL PREVIOUS MEMORY OPERATIONS ARE FLUSHED AND SYNCHRONIZED ACROSS ALL CORES AND AN ATOMIC LOAD CONNECTS THE ORDERING OF DIFFERENT THREADS ESSENTIALLY APPENDING THE ORDERING AFTER THE LOAD IN ONE THREAD TO THE ORDERING BEFORE THE STORE IN THE OTHER THREAD. 

    - intra thread ordering (before stores), inter thread ordering (when connecting stores to loads)
    - sequential consistent (default): no reordering, all threads see atomic operations in the exact same order, seq_cst forces all cores to agree on one single, universal sequence of events for all atomic variables
    - acquire release order: captures only relationship between specific threads, no global order

- Question: "What is the difference of single-core CPUs and multi-core CPUs with respect to std::atomics?"
    - On multi-core CPUs cache coherence is enforced by locking cache lines and threads need to wait until the other core is done writing the value. On single-core CPUs cache coherence problems are impossible because there only exist caches on one CPU. However, reordering by the CPU is prevented and protection against OS-level instruction interleaving via context switches is ensured in both cases. 

- Question: "Why are std::atomics faster than mutexes?"
    - the compiler generates single CPU instructions for atomics and the synchronization is handled by the hardware and cache controllers of the CPU, the thread does not stop running. when using mutexes threads are put to sleep when waiting for synchronization.

### atomic operations
- compare_exchange_weak() can fail spuriously, i.e. the store is not successful, because on some processors there is no single atomic compare-and-exchange instruction and the processor cannot guarantee that the instructions were performed atomically. it should therefore be used in a loop. 

### synchronizing operations and enforcing ordering




## lock-based concurrent data structures
todo

## lock-free concurrent data structures
todo
