#pragma once
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <thread>
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

  /**
   * @brief Adaptive blocking primitives (spin-then-block)
   *
   * These members provide a low-overhead mechanism to block the producer
   * or consumer when waits become long. The strategy is:
   *  1. Busy-spin for a short number of iterations to cover common short
   *     wait periods (keeps latency low for brief stalls).
   *  2. If spinning exceeds a threshold, enter a condition_variable wait to
   *     yield the CPU until the opposite side signals progress.
   *
   * The implementation is careful to preserve the single-producer /
   * single-consumer assumptions of the ring: only at most one thread will
   * ever wait on the producer-side path and at most one on the consumer
   * side. Using a mutex + condvar here simplifies correctness and avoids
   * more complex lock-free blocking constructs.
   */
  mutable std::mutex      cv_mutex_;     /**< Mutex for condition variables */
  std::condition_variable not_empty_cv_; /**< Notifies consumer when items arrive */
  std::condition_variable not_full_cv_;  /**< Notifies producer when space freed */

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

  /**
   * @brief Push with adaptive spin-then-block waiting
   *
   * This call will attempt to push the supplied value into the ring. It
   * first busy-spins for up to `spin_limit` failed attempts to keep
   * latency low for short waits. If the ring remains full it will then
   * block on a condition variable until space becomes available.
   *
   * @tparam ENQ_TYPE Deduced type for the value to insert
   * @param value Value to insert (forwarded)
   * @param spin_limit Number of spin attempts before blocking (default 1024)
   * @param spin_counter Optional pointer to atomic counter to accumulate spin attempts
   * @param block_counter Optional pointer to atomic counter incremented on each block
   */
  template<typename ENQ_TYPE>
  void push_wait(ENQ_TYPE &&value,
      size_t                spin_limit    = 1024,
      std::atomic<size_t>  *spin_counter  = nullptr,
      std::atomic<size_t>  *block_counter = nullptr) noexcept {
    // Local accumulator for spins to avoid frequent atomic updates.
    size_t local_spins = 0;

    // Try until push succeeds. The fastpath uses the existing non-blocking
    // push() which is optimized for the SPSC case.
    while (true) {
      if (push(std::forward<ENQ_TYPE>(value))) {
        // Successful push: update statistics and notify the consumer.
        if (spin_counter && local_spins) {
          spin_counter->fetch_add(local_spins, std::memory_order_relaxed);
        }
        // Notify without holding the mutex to avoid unnecessary contention.
        not_empty_cv_.notify_one();
        return;
      }

      ++local_spins;
      // Short exponential backoff is not necessary here; yield occasionally
      // to reduce busyness while still remaining responsive.
      if (local_spins < spin_limit) {
        if ((local_spins & 0x3FF) == 0) std::this_thread::yield();
        continue;
      }

      // Spinning exceeded threshold: enter blocking wait. We increment the
      // block counter once per blocking entry to let benchmarks observe
      // how often threads actually block.
      if (block_counter) block_counter->fetch_add(1, std::memory_order_relaxed);

      std::unique_lock<std::mutex> lock(cv_mutex_);
      // Wait until not full. The predicate checks the ring state so spurious
      // wakeups are harmless.
      not_full_cv_.wait(lock, [this]() { return !isFull(); });

      // After wakeup, loop and attempt push again. Reset local spin counter
      // to account for spins after wakeup.
      local_spins = 0;
    }
  }

  /**
   * @brief Pop with adaptive spin-then-block waiting
   *
   * Symmetric to push_wait(): busy-spins for `spin_limit` failed attempts
   * to pop, then blocks on a condition variable if the ring remains empty.
   *
   * @param out Reference that receives the popped element
   * @param spin_limit Number of spin attempts before blocking (default 1024)
   * @param spin_counter Optional pointer to atomic counter to accumulate spin attempts
   * @param block_counter Optional pointer to atomic counter incremented on each block
   * @return true on successful pop (always returns true eventually)
   */
  bool pop_wait(Q_TYPE    &out,
      size_t               spin_limit    = 1024,
      std::atomic<size_t> *spin_counter  = nullptr,
      std::atomic<size_t> *block_counter = nullptr) noexcept {
    size_t local_spins = 0;

    while (true) {
      if (pop(out)) {
        if (spin_counter && local_spins) {
          spin_counter->fetch_add(local_spins, std::memory_order_relaxed);
        }
        // Wake producer that space is available.
        not_full_cv_.notify_one();
        return true;
      }

      ++local_spins;
      if (local_spins < spin_limit) {
        if ((local_spins & 0x3FF) == 0) std::this_thread::yield();
        continue;
      }

      if (block_counter) block_counter->fetch_add(1, std::memory_order_relaxed);
      std::unique_lock<std::mutex> lock(cv_mutex_);
      not_empty_cv_.wait(lock, [this]() { return !isEmpty(); });
      local_spins = 0;
    }
  }
};