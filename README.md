# Tiered Concurrent Arena Allocator

A high-performance, tiered concurrent memory allocator written in pure C for low-latency, multi-threaded workloads with phased lifecycles—such as network packet-processing data planes or real-time game engines.

By decoupling thread-local allocation from a centralized coordinator, worker threads allocate memory completely **lock-free**, achieving an average latency of **3.09 nanoseconds**—outperforming system `glibc malloc` by **6.7x**.

---


## 📊 Performance Benchmarks

The engine was evaluated on a Linux x86_64 system under a **16-core parallel workload**.

### 📈 Test Profile: Production Lifecycle Mode (Phased Loops)
*Workload: 16 parallel threads running 2,000 sequential loops, executing 250 dynamic allocations (sizes matching network payloads up to 2KB) per loop.*

| **Subsystem Layer** | **Total Duration** | **Time Per Allocation** | **Performance Delta** |
| :--- | :--- | :--- | :--- |
| **Our Tiered Arena** | **24.70 ms** | **3.09 ns** | **6.7x Faster (Winner)** |
| System `glibc malloc` | 166.84 ms | 20.86 ns | Baseline |
---

## 🔌 API Integration Reference

Integrating this allocator into an existing C/C++ project is seamless. Include the headers and spin up the architecture using these primary modules:

### 1. Global Backend Lifecycle
Configure the centralized coordinator subsystem when your application boots up.

```c
#include "global_pool_manager.h"

// Instantiate the global singleton coordinator
pthread_mutex_t global_lock = PTHREAD_MUTEX_INITIALIZER;
global_pool_manager* manager = init_global_pool_manager(&global_lock);

// Entirely dismantle the allocator and return all virtual memory blocks back to the OS
cleanup_global_pool_manager();
```

### 2. Thread-Local Frontend Lifecycle
Every concurrent worker thread initializes its private arena context inside its execution loop.

```c
#include "thread_arena.h"

// Allocate and initialize an isolated, thread-local workspace
thread_arena* my_arena = thread_init_thread_arena();

// High-speed pointer bump allocation (Guarantees strict 8-byte hardware alignment)
void* dynamic_payload = thread_arena_alloc(my_arena, size_in_bytes);

// Complete thread teardown: returns all owned blocks cleanly back to the global pool
thread_cleanup_thread_arena(my_arena);
```

### 3. The Production Cycle Reset (Phase Shift)
This is the core optimization API. Use it at the end of a processing frame or request window to wipe temporary data without losing execution speed.

```c
// Resets the pointer offset of your active arena block back to 0 instantly.
// Recycles any extra overflowing blocks back to the global manager O(1) pool.
thread_cleanup_thread_arena_except_current(my_arena);
```
