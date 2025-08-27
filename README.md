# RingMaster · High-Performance SPSC Ring Buffer

![Build Status](https://img.shields.io/badge/status-working-green)
![Linux Build](https://img.shields.io/badge/Platform-Linux-blue.svg)
![License](https://img.shields.io/badge/license-MIT-blue)
[![Language](https://img.shields.io/badge/Language-C%2B%2B20-lightgrey.svg)](https://isocpp.org/)
[![Contributions Welcome](https://img.shields.io/badge/Contributions-Welcome-brightgreen.svg)](https://github.com/Vaibhav-20022002/RingMaster/issues)

> High-performance SPSC queue with a lock-free core and efficient adaptive waiting.

**RingMaster** is a header-only, single-producer/single-consumer (SPSC) ring buffer engineered for high-throughput, low-latency communication between threads. Its core is a pure lock-free design, now enhanced with an adaptive waiting strategy to provide maximum performance with minimal CPU waste.

-----

## Key Advantages

  * **Atomic, Lock-Free Core**: Guarantees safe, non-blocking handoffs between one producer and one consumer using only `std::atomic` indices and precise memory ordering (`release`/`acquire`).
  * **Adaptive Spin-then-Block Waiting**: New `push_wait` and `pop_wait` methods provide a highly efficient waiting strategy. Threads first spin for a short duration to handle transient contention, then block on a condition variable to yield the CPU, eliminating wasted cycles during longer waits.
  * **Cache-Line Alignment**: `head` and `tail` counters are padded to fill separate cache lines, and the storage array is aligned to the CPU’s cache line size to obliterate false sharing.
  * **Header-Only**: Include `RingMaster.hh`—no link step, no dependencies, zero boilerplate.
  * **Automatic Cache Detection**: The provided CMake script runs a utility to discover your system’s cache line size and injects it as a compile-time constant for optimal alignment.
  * **High Throughput**: Achieves millions of operations per second for small items and is capable of saturating DRAM bandwidth for large payloads.

-----

## Performance Snapshot

The following results showcase the performance of the new **adaptive spin-then-block** strategy.

**Test Environment:**

| Property | Value |
| :--- | :--- |
| **CPU** | Intel® Core™ i7-6600U @ 2.60 GHz (Skylake) |
| **Cores / Threads** | 2 Cores / 4 Threads |
| **Max Turbo** | 3.40 GHz |
| **Compiler** | Clang (O0–O3) |
| **Buffer Capacity** | 512 Items |
| **Total Items** | 10,000,000 |

-----

**Results:**

This table shows the best average performance for each element size across all optimization levels.

| Element | Opt Level | Throughput (items/s) | Bandwidth (MB/s) | Push Spin (%) | Pop Spin (%) | Push Block (%) | Pop Block (%) | Efficiency (items/µs) |
|:-------:|:---------:|---------------------:|-----------------:|--------------:|-------------:|---------------:|--------------:|----------------------:|
| 4 bytes | O2 | 19,864,880 | 75.78 | 14.88 | 9.87 | 0.0064 | 0.0060 | 19.86 |
| 8 bytes | O3 | 18,615,502 | 142.02 | 4.02 | 22.02 | 0.0002 | 0.0006 | 18.62 |
| 16 bytes | O3 | 21,773,068 | 332.23 | 4.79 | 30.56 | 0.0003 | 0.0006 | 21.77 |
| 32 bytes | O2 | 19,162,110 | 584.78 | 21.31 | 24.57 | 0.0004 | 0.0005 | 19.16 |
| 64 bytes | O3 | 17,212,382 | 1,050.57 | 6.84 | 50.81 | 0.0002 | 0.0006 | 17.21 |
| 1 KB | O3 | 11,460,931 | 11,192.32 | 0.44 | 0.55 | 0.0001 | 0.0006 | 11.46 |
| 2 KB | O3 | 7,719,832 | 15,077.80 | 1.13 | 0.73 | 0.0003 | 0.0011 | 7.72 |
| 4 KB | O3 | 5,186,544 | 20,259.94 | 3.63 | 6.01 | 0.0009 | 0.0051 | 5.19 |

> **Observation**: The adaptive waiting strategy proves highly effective. The number of blocking events is exceptionally low, indicating the spin-wait phase handles most contention. For large payloads, the system becomes memory-bound, with performance limited by DRAM bandwidth, which the buffer handles without wasting CPU cycles.

Full analysis: [benchmarks/README.md](benchmarks/README.md)

-----

## Getting Started

### 1\. Clone & Build

```bash
git clone https://github.com/Vaibhav-20022002/RingMaster.git
cd RingMaster
mkdir build && cd build
cmake ..                  # automatic cache line detection
make -j$(nproc)
```

You’ll see:

```
-- Successfully detected cache line size: 64 bytes. (or whatever your system has)
```

### 2\. Include in Your Project

No linking necessary. Just:

```cpp
#include "RingMaster.hh"

// Define a buffer for 1024 elements of MyType
RingMaster<MyType, 1024> buffer;
```

### 3\. Sample Usage

The new `push_wait` and `pop_wait` methods provide a simple and efficient blocking API.

```cpp
#include "RingMaster.hh"
#include <thread>

// Producer thread pushes items into the buffer, waiting if it's full.
void producer(RingMaster<int, 1024>& buf) {
    for (int i = 0; i < 10000; ++i) {
        buf.push_wait(i);
    }
}

// Consumer thread pops items, waiting if the buffer is empty.
void consumer(RingMaster<int, 1024>& buf) {
    int out;
    for (int i = 0; i < 10000; ++i) {
        buf.pop_wait(out);
        // process(out);
    }
}

int main() {
    RingMaster<int, 1024> buf;
    std::thread p(producer, std::ref(buf));
    std::thread c(consumer, std::ref(buf));
    p.join();
    c.join();
    return 0;
}
```

-----

## How It Works

1.  **Circular Indexing**: `head` (write index) and `tail` (read index) are atomic counters. A power-of-two capacity allows for efficient wrap-around using a bitmask.
2.  **Memory Ordering**: The non-blocking `push()` uses `std::memory_order_release` on its store to make the write visible to the consumer, while `pop()` uses `std::memory_order_acquire` on its load to see the write, ensuring data is safely transferred.
3.  **False-Sharing Defense**: Padded atomics and an aligned buffer ensure that the `head` index, `tail` index, and the buffer itself do not share cache lines, preventing performance degradation from CPU cache coherency protocols.
4.  **Adaptive Waiting**: The `push_wait` and `pop_wait` methods use a hybrid strategy. They first attempt to push/pop in a tight spin loop. If the buffer remains full/empty after a set number of spins, the thread acquires a mutex and blocks on a `std::condition_variable`, yielding the CPU until it is signaled.

-----

## Assumptions & Limitations

  * **SPSC only**: The data structure is not safe for multiple producers or multiple consumers.
  * **Power-of-two Capacity**: Required for efficient bitmask-based indexing.
  * **Movable Types**: The element type `Q_TYPE` must support nothrow move construction and assignment.

-----

## Project Structure

```
.
├── benchmarks/
│   └── README.md         # Detailed results & plots
├── build/                # CMake out-of-source build
├── tools/
│   └── cacheLineSize.cc  # Utility for cache detection
├── RingMaster.hh         # Header-only implementation
├── demo.cpp              # Example code
└── CMakeLists.txt        # Build script
```

-----

## Roadmap

  * **Batch APIs**: Implement `push_bulk` and `pop_bulk` for transferring multiple elements at once to improve efficiency.
  * **Runtime Metrics**: Expose internal metrics like buffer occupancy and contention rates for monitoring.
  * **Dynamic Capacity**: Explore options for a ring buffer with a capacity that can be set at runtime.

-----

## License

Licensed under the **MIT License**. See [LICENSE](LICENSE) for details.
