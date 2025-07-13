#pragma once
#include <atomic>
#include <cstddef>
#include <memory>
#include <utility>

/**
 * @brief Lock-free ring buffer implementation for high-performance
 * producer/consumer workflows
 *
 * This header defines the RingMaster template class, providing a
 * fixed-capacity, cache-aligned, lock-free circular buffer. It targets
 * single-producer/single-consumer use cases, minimizing overhead by avoiding
 * locks and employing atomic operations with appropriate memory ordering to
 * guarantee thread safety.
 *
 * @section Assumptions
 * - Capacity is compile-time constant and a power of two for efficient
 * indexing.
 * - Only one thread calls push(), and only one thread calls pop().
 * - Type Q_TYPE supports nothrow move construction and assignment; copying is
 * avoided.
 *
 * @section Usage
 * @code
 * RingMaster<MyType> buffer;
 * MyType item;
 * if (buffer.push(std::move(item))) {
 *     // pushed successfully
 * }
 * MyType out;
 * if (buffer.pop(out)) {
 *     // popped successfully
 * }
 * @endcode
 *
 * @tparam Q_TYPE Element type stored in the ring; must be movable without
 * throwing.
 */

#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 64 // Default cache line size in bytes
#endif

/**
 * @class RingMaster
 * @brief Lock-free circular buffer for single-producer/single-consumer patterns
 *
 * RingMaster provides constant-time push and pop operations with minimal
 * latency. Internally, it uses two atomic counters (head_ and tail_) padded to
 * cache lines to prevent false sharing. The buffer array is also aligned
 * to 64-byte boundaries.
 *
 * @note This class is NOT safe for multiple concurrent producers or consumers.
 * @warning clear() is not thread-safe; only call when no push/pop is in flight.
 */
template<typename Q_TYPE, size_t Capacity> class RingMaster {
  static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of two");

  // Mask for wrap-around indexing
  static constexpr size_t Mask = Capacity - 1;

private:
  /**
   * @struct PaddedAtomic
   * @brief Cache-aligned atomic counter to avoid false sharing
   *
   * Each atomic variable is padded to occupy its own cache line,
   * preventing contention between threads operating on head_ and tail_.
   */
  struct alignas(CACHE_LINE_SIZE) PaddedAtomic {
    std::atomic<size_t> var; /**< Atomic index counter (head or tail) */
    char pad[CACHE_LINE_SIZE - sizeof(std::atomic<size_t>)]; /**< Padding to complete cache
                                                   line size */
  };

  PaddedAtomic head_{0}; /**< Producer index (next write position) */
  PaddedAtomic tail_{0}; /**< Consumer index (next read position) */
  alignas(
      CACHE_LINE_SIZE) Q_TYPE buffer_[Capacity]; /**< Storage for elements, cache-line aligned */

public:
  /**
   * @brief Default constructor initializes indices
   *
   * head_ and tail_ start at zero, indicating an empty buffer.
   */
  RingMaster() noexcept = default;

  /**
   * @brief Destructor cleans up resources
   *
   * No dynamic allocation is used; default behavior suffices.
   */
  ~RingMaster() { clear(); }

  // Non-copyable, non-movable
  RingMaster(const RingMaster &)            = delete;
  RingMaster &operator=(const RingMaster &) = delete;

  /**
   * @brief Push an element into the ring buffer if space is available
   *
   * Uses atomic operations to update head position after storing the value.
   * Employs perfect forwarding to accept lvalues or rvalues efficiently.
   *
   * @tparam ENQ_TYPE Type deduced for insertion (should match Q_TYPE or
   * convertible)
   * @param value Element to insert (forwarded)
   * @return true if insertion succeeded, false if buffer was full
   */
  template<typename ENQ_TYPE> bool push(ENQ_TYPE &&value) noexcept {
    const size_t head = head_.var.load(std::memory_order_relaxed);
    const size_t tail = tail_.var.load(std::memory_order_acquire);

    if (head - tail >= Capacity) { // buffer full
      return false;
    }

    buffer_[head & Mask] = std::forward<ENQ_TYPE>(value);

    // Use release ordering to ensure the data write is visible before the head update
    head_.var.store(head + 1, std::memory_order_release);
    return true;
  }

  /**
   * @brief Pop the oldest element from the buffer
   *
   * Retrieves the element at tail_, moves it into `out`, then advances tail_.
   *
   * @param out Reference where the popped element is stored
   * @return true if an element was available, false if buffer was empty
   */
  bool pop(Q_TYPE &out) noexcept {
    const size_t tail = tail_.var.load(std::memory_order_relaxed);
    const size_t head = head_.var.load(std::memory_order_acquire);

    if (head == tail) { // buffer empty
      return false;
    }

    out = std::move(buffer_[tail & Mask]);

    // Use release ordering to ensure data read completes before tail update
    tail_.var.store(tail + 1, std::memory_order_release);
    return true;
  }

  /**
   * @brief Discard up to n oldest elements without retrieval
   *
   * Advances tail_ by up to n positions, effectively removing elements.
   *
   * @param n Maximum number of elements to remove
   * @return Actual number of elements removed
   */
  size_t remove(size_t n) noexcept {
    const size_t tail     = tail_.var.load(std::memory_order_relaxed);
    const size_t head     = head_.var.load(std::memory_order_acquire);
    const size_t avail    = head - tail;
    const size_t toRemove = (n > avail) ? avail : n;

    if (toRemove) {
      tail_.var.store(tail + toRemove, std::memory_order_release);
    }
    return toRemove;
  }

  /**
   * @brief Reset buffer to empty state
   *
   * Sets both head_ and tail_ back to zero.
   *
   * @warning Not thread-safe. Only call when no concurrent push/pop operations.
   */
  void clear() noexcept {
    head_.var.store(0, std::memory_order_relaxed);
    tail_.var.store(0, std::memory_order_relaxed);
  }

  /**
   * @brief Check if buffer is empty
   *
   * Compares head_ and tail_ atomically; may be slightly stale under
   * concurrency.
   *
   * @return true if no elements are in the buffer, false otherwise.
   */
  bool isEmpty() const noexcept {
    const size_t head = head_.var.load(std::memory_order_acquire);
    const size_t tail = tail_.var.load(std::memory_order_acquire);
    return head == tail;
  }

  /**
   * @brief Check if buffer is full
   *
   * True when advancing head_ would collide with tail_.
   *
   * @return true if no additional elements can be pushed, false otherwise.
   */
  bool isFull() const noexcept {
    const size_t head = head_.var.load(std::memory_order_acquire);
    const size_t tail = tail_.var.load(std::memory_order_acquire);
    return head - tail >= Capacity;
  }

  /**
   * @brief Get approximate count of elements in buffer
   *
   * Computes difference between head_ and tail_; may be stale under heavy
   * concurrency.
   *
   * @return Number of elements currently held in buffer
   */
  size_t size() const noexcept {
    const size_t head = head_.var.load(std::memory_order_acquire);
    const size_t tail = tail_.var.load(std::memory_order_acquire);
    return head - tail;
  }
};