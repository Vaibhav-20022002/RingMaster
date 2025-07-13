# RingMaster · Lock-Free SPSC Ring Buffer

![Build Status](https://img.shields.io/badge/status-working-green)
![Linux Build](https://img.shields.io/badge/Platform-Linux-blue.svg)
![License](https://img.shields.io/badge/license-MIT-blue)
[![Language](https://img.shields.io/badge/Language-C%2B%2B20-lightgrey.svg)](https://isocpp.org/)
[![Contributions Welcome](https://img.shields.io/badge/Contributions-Welcome-brightgreen.svg)](https://github.com/Vaibhav-20022002/RingMaster/issues)

> **Drop-in simplicity. Lock-free performance.**

**RingMaster** is a header-only, single-producer/single-consumer (SPSC) ring buffer for modern C++. Just include `RingMaster.hh`, and you're ready to blast through millions of items per second—no locks, no fuss. It's designed from the ground up for extreme speed in concurrent workflows by eliminating lock overhead and minimizing cache contention.

---

# ✨ Core Features

  - **Lock-Free Design**: Guarantees thread safety for SPSC scenarios without slow mutexes or kernel traps, ensuring predictable, low latency.
  - **Cache-Optimized**: Intelligently pads and aligns internal structures (`head`, `tail`, and the buffer itself) to the CPU's cache line size, **preventing "false sharing"** and maximizing performance.
  - **Automatic Cache Detection**: The CMake build system automatically detects your system's cache line size and injects it as a compile-time constant for optimal alignment.
  - **Header-Only**: Simply include `RingMaster.hh` in your project. No libraries to build or link.
  - **Blazing Fast**: Achieves exceptional throughput and bandwidth, making it ideal for latency-sensitive applications.

---

# 🚀 Performance Highlights

Benchmarked with **10 million items** and a buffer capacity of **512**. RingMaster demonstrates impressive throughput, especially for elements that fit within CPU caches.

| Element | Optimal Opt | Throughput (items/s) | Push Retries | Pop Retries | Efficiency (items/µs) |
| :---: | :-------: | :----------------: | :------: | :-------: | :-------------: |
| **8 B** |     O3      | **65,211,154** (497 MB/s)  |    10.3%     |    22.8%    |       **65.21** |
| **32 B**|     O2      |    58,012,043 (1.77 GB/s)  |    12.8%     |    35.0%    |         58.01         |
| **64 B**|     O2      |    53,600,909 (3.27 GB/s)  |  **0.37%** |    26.8%    |         53.60         |
| **4 KB**|     O3      |    7,064,771 (**27.6 GB/s**) |    0.00%     |  **98.5%** |         7.06          |

> *Peak sweet spot is for `8B` – `64B` elements under `O2`/`O3` optimization. Larger blocks quickly become memory-bandwidth bound, with RingMaster effectively saturating DDR4 speeds.*

For a complete analysis, see the [**detailed benchmark summary**](https://www.google.com/search?q=./benchmarks/benchmark.md).

---

# Getting Started

## `Quick Start`

Using RingMaster is simple. Include the header, create an instance, and you're ready to go.

```cpp
#include "RingMaster.hh"
#include <thread>

// Create a buffer holding 1024 elements of MyPayload
RingMaster<MyPayload, 1024> buffer;

// --- Producer Thread ---
void producer() {
    MyPayload p{ /* ... */ };
    // Push will return false if the buffer is full
    while (!buffer.push(std::move(p))) {
        std::this_thread::yield(); // Wait for space
    }
}

// --- Consumer Thread ---
void consumer() {
    MyPayload c;
    // Pop will return false if the buffer is empty
    if (buffer.pop(c)) {
        // Got one!
    }
}
```

## `Building with CMake` (Recommended)

The recommended method uses CMake to **automatically detect your CPU's cache line size** for the best performance.

1.  **Clone and create a build directory:**

    ```sh
    git clone <your-repo-url> && cd <repo-folder>
    mkdir build && cd build
    ```

2.  **Run CMake:**

    ```sh
    cmake ..
    ```

    You'll see the cache-line detection in action:

    ```
    -- Successfully detected cache line size: 64 bytes.
    ```

3.  **Build the project:**

    ```sh
    make -j$(nproc)
    ```

---

# How It Works

RingMaster uses a circular buffer where `head` is the write index (producer) and `tail` is the read index (consumer).

```
       +-------------------------+
 Head→ | slot0 | slot1 | ... | slotN-1 | slot0 | ←Tail
       +-------------------------+
             ^                       ^
          write                   read
```

Its speed comes from two key lock-free principles:

1.  **Atomic Indices with Memory Ordering**: The `head` and `tail` indices are `std::atomic`. The producer only modifies `head` and the consumer only modifies `tail`. `std::memory_order_release` (on push) and `std::memory_order_acquire` (on pop) create a synchronization point, ensuring data writes are visible to the consumer *before* the index is updated.

2.  **Cache Line Padding**: To prevent **false sharing** (where threads contend for the same cache line even when accessing different variables), the `head` and `tail` atomics are each padded to fill an entire cache line. This guarantees they reside on separate cache lines, allowing the CPU cores to operate on them in parallel without cache-coherency conflicts.

---

# Use Cases

  - **Audio/Video Streaming**: Pass fixed-size frames between processing threads.
  - **Low-Latency Networking**: Queue incoming/outgoing packets without OS lock overhead.
  - **Real-Time Analytics**: Feed data from an ingest thread to worker threads at a high cadence.
  - **Game Development**: Decouple game logic and rendering threads for smoother frame rates.
  - **Telemetry Collectors**: Ingest millions of small messages from multiple sources into a processing pipeline.

---

# Roadmap

  - **Bulk Operations**: Add `push_bulk` / `pop_bulk` APIs for efficient batch processing.
  - **Runtime Stats**: Expose metrics like occupancy and contention rates.
  - **Adaptive Back-off**: Implement smart spin-waiting strategies for high-contention scenarios.
  - **Dynamic Sizing**: Explore a version that doesn't require a compile-time, power-of-two capacity.

---

## ⚠️ Assumptions & Limitations

  - **Single-Producer/Single-Consumer ONLY**: This implementation is **not safe** for multiple producers or multiple consumers. Its design is strictly optimized for the SPSC use case.
  - **Power-of-Two Capacity**: The buffer's capacity **must be a power of two** (e.g., 256, 512, 1024) for the high-speed bit-mask indexing to work.
  - **Movable Elements**: The type `Q_TYPE` stored in the buffer must be movable (`std::move`).

---

## 📜 License

This project is licensed under the **MIT License**. See the [LICENSE](https://www.google.com/search?q=./LICENSE) file for details.