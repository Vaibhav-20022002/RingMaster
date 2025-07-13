# RingMaster · Lock-Free SPSC Ring Buffer

![Build Status](https://img.shields.io/badge/status-working-green)
![Linux Build](https://img.shields.io/badge/Platform-Linux-blue.svg)
![License](https://img.shields.io/badge/license-MIT-blue)
[![Language](https://img.shields.io/badge/Language-C%2B%2B20-lightgrey.svg)](https://isocpp.org/)
[![Contributions Welcome](https://img.shields.io/badge/Contributions-Welcome-brightgreen.svg)](https://github.com/Vaibhav-20022002/RingMaster/issues)

> **Radical performance. Zero locks. Millisecond-scale latencies.**

**RingMaster** is a header-only, single-producer/single-consumer ring buffer engineered from first principles for maximal throughput and minimal latency. No mutexes. No syscalls. Just raw, lock-free C++ at the hardware boundary.

---

## ✨ Key Advantages

* **Atomic, Lock-Free Core**: Guarantees safe handoff between one producer and one consumer using only `std::atomic` indices and precise memory ordering (`release`/`acquire`).
* **Cache-Line Alignment**: `head` and `tail` counters are each padded to fill a cache line. The storage array is aligned to your CPU’s cache line size (detected at build time) to obliterate false sharing.
* **Header-Only**: Include `RingMaster.hh`—no link step, no dependencies, zero boilerplate.
* **Automatic Cache Detection**: The provided CMake script compiles and runs a tiny utility to discover your system’s cache line size, then injects it as a compile-time constant.
* **Blistering Throughput**: Millions of operations per second for small items; saturates DRAM bandwidth for large payloads.

---

## 🚀 Performance Snapshot

**Test Environment:**

| Property            | Value                                      |
| ------------------- | ------------------------------------------ |
| **CPU**             | Intel® Core™ i7-6600U @ 2.60 GHz (Skylake) |
| **Cores / Threads** | 2 Cores / 4 Threads                        |
| **Max Turbo**       | 3.40 GHz                                   |
| **Compiler**        | Clang (O0–O3)                              |
| **Buffer Capacity** | 512 Items                                  |
| **Total Items**     | 10,000,000                                 |

---
**Results:**

| Element | Opt Level | Throughput (items/s) | Bandwidth (MB/s) | Push Retry (%) | Pop Retry (%) | Efficiency (items/μs) |
| :-----: | :-------: | -------------------: | ---------------: | -------------: | ------------: | --------------------: |
|   4 B   |     O2    |           57,289,847 |           218.54 |          11.13 |         26.27 |                 57.29 |
|   8 B   |     O2    |           63,579,258 |           485.07 |          13.13 |         31.41 |                 63.58 |
|   16 B  |     O2    |           62,606,823 |           955.37 |          12.68 |         31.63 |                 62.61 |
|   32 B  |     O2    |           58,177,079 |         1,775.42 |           5.26 |         14.37 |                 58.18 |
|   64 B  |     O1    |           54,691,431 |         3,338.10 |          12.54 |         50.50 |                 54.69 |
|   1 KB  |     O2    |           18,047,707 |        17,624.71 |           0.19 |         91.28 |                 18.05 |
|   2 KB  |     O2    |           12,421,373 |        24,260.49 |           0.93 |         96.80 |                 12.42 |
|   4 KB  |     O2    |            7,034,496 |        27,478.50 |           0.00 |         98.56 |                  7.03 |

> **Observation**: Peak efficiency at 8 B–32 B under O2; large blocks become memory-bound, saturating DDR4 bandwidth.

Full analysis: [benchmarks/benchmark.md](benchmarks/benchmark.md)

---

## Getting Started

### 1. Clone & Build

```bash
git clone https://github.com/YourRepo/RingMaster.git
cd RingMaster
mkdir build && cd build
cmake ..                  # automatic cache line detection
make -j$(nproc)
```

You’ll see:

```
-- Successfully detected cache line size: 64 bytes. (or whatever system has)
```

### 2. Include in Your Project

No linking necessary. Just:

```cpp
#include "RingMaster.hh"

// Define a buffer for 1024 elements of MyType
RingMaster<MyType, 1024> buffer;
```

### 3. Sample Usage

```cpp
#include <thread>

void producer(RingMaster<int, 1024>& buf) {
    int val = 0;
    while (!buf.push(val++)) {
        std::this_thread::yield();
    }
}

void consumer(RingMaster<int, 1024>& buf) {
    int out;
    while (buf.pop(out)) {
        process(out);
    }
}

int main() {
    RingMaster<int, 1024> buf;
    std::thread p(producer, std::ref(buf));
    std::thread c(consumer, std::ref(buf));
    p.join(); c.join();
}
```

---

##  How It Works

1. **Circular Indexing**: `head` (write) and `tail` (read) are atomic counters modulo `Capacity` (power-of-two mask).
2. **Memory Ordering**: `push()` uses `memory_order_relaxed` for loads, `release` for the store; `pop()` uses `acquire` for the load and `release` for the store.
3. **False-Sharing Defense**: Padded atomics and aligned buffer ensure no two variables share a cache line.

---

## 🚧 Assumptions & Limitations

* **SPSC only**: Not safe for multiple producers or consumers.
* **Power-of-two Capacity**: Required for bitmask wrap-around.
* **Movable Types**: `Q_TYPE` must support nothrow move construction/assignment.

---

## Project Structure

```
.
├── benchmarks/
│   ├── benchmark.md   # Detailed results & plots
│   └── README.md      # Bench summary overview
├── build/             # CMake out-of-source build
├── tools/
│   └── cacheLineSize.cc  # Utility for cache detection
├── RingMaster.hh      # Header-only implementation
├── main.cc            # Example usage
└── CMakeLists.txt     # Build script
```

---

## Roadmap

* **Batch APIs**: `push_bulk` / `pop_bulk` for multi-element transfers.
* **Runtime Metrics**: Expose occupancy, contention rates.
* **Adaptive Spin-Wait**: Dynamic back-off to minimize CPU waste under contention.
* **Dynamic Capacity**: Explore non-compile-time sizing options.

---

## License

Licensed under the **MIT License**. See [LICENSE](LICENSE) for details.
