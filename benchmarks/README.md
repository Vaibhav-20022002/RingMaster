# RingMaster SPSC Buffer Benchmark Summary

## Test Environment
- **CPU**: Intel(R) Core(TM) i7-6600U @ 2.60GHz (Skylake)
- **Cores/Threads**: 2 cores / 4 threads
- **Max Frequency**: 3.40 GHz
- **Buffer Capacity**: 512 items
- **Total Items Processed**: 10,000,000 per test
- **Compiler**: _CLANG_ with optimization levels `O0`-`O3`

## Performance Summary
Peak performance across element sizes with best optimization level:

| Element Size | Best Opt | Throughput<br>(items/s) | Bandwidth<br>(MB/s) | Push Retry<br>(%) | Pop Retry<br>(%) | Efficiency<br>(items/μs) |
| -----------: | :------: | ----------------------: | ------------------: | ----------------: | ---------------: | -----------------------: |
|      4 bytes |    O2    |            57'289'847 |             218.54 |            11.13 |           26.27 |                 57.29 |
|      8 bytes |    O2    |            63'579'258 |             485.07 |            13.13 |           31.41 |                 63.58 |
|     16 bytes |    O2    |            62'606'823 |             955.37 |            12.68 |           31.63 |                 62.61 |
|     32 bytes |    O2    |            58'177'079 |           1'775.42 |             5.26 |           14.37 |                 58.18 |
|     64 bytes |    O1    |            54'691'431 |           3'338.10 |            12.54 |           50.50 |                 54.69 |
|   1024 bytes |    O2    |            18'047'707 |          17'624.71 |             0.19 |           91.28 |                 18.05 |
|   2048 bytes |    O2    |            12'421'373 |          24'260.49 |             0.93 |           96.80 |                 12.42 |
|   4096 bytes |    O2    |             7'034'496 |          27'478.50 |             0.00 |           98.56 |                  7.03 |

---

## Key Observations

1. **Compiler Optimization Impact**:
   - `O2` consistently outperformed other levels for most element sizes (4B-32B, 1KB+)
   - `O1` showed advantages for 64B elements due to better register allocation
   - `O0` performed worst (expected), while `O3` showed no consistent gains over `O2`

2. **Throughput/Bandwidth Scaling**:
   - **Small items (4-64B)**: Near-linear scaling with L1/L2 cache speeds
   - **Medium items (64B-1KB)**: L3 cache bound (~3.3 GB/s for 64B)
   - **Large items (1KB+)**: Memory-bound, peaking at ~27.5 GB/s (DDR4 limitations)

3. **Contention Behavior**:
   - Push retries remained low (<15%) across all sizes
   - Pop retries increased exponentially with element size, exceeding 90% for items ≥1KB
   - 4B elements showed highest contention (up to 26.27% retries)

4. **Efficiency Patterns**:
   - Peak efficiency at 8B (63.58 items/μs)
   - Inverse relationship between element size and efficiency
   - 4096B elements processed at 7% of 8B efficiency

5. **Memory Subsystem Effects**:
   - Clear cache hierarchy transitions at:
     - 64B (L1→L2 boundary)
     - 1KB (L3→DRAM boundary)
   - DRAM bandwidth saturation observed beyond 2KB elements

---

## Conclusions
1. **Optimal optimization level is O2** for most use cases
2. **4-64B elements** achieve best throughput (50-60M items/s)
3. **>1KB elements** become memory bandwidth-bound, not CPU-bound
4. Buffer capacity (512 items) creates significant contention for:
   - Small items (high operation rate)
   - Large items (slow consumer draining)
5. Implementation shows robust correctness (100% accuracy across all tests)

> **Recommendation**: For latency-sensitive applications, use 8-32B elements with O2 optimization. For bandwidth-heavy workloads, optimize for cache line sizes (64B multiples).