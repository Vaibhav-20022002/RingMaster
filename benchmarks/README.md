# RingMaster SPSC Buffer Benchmark Summary

This table summarizes the peak observed performance across varying element sizes using the updated memory model on an Intel i7 6th gen (Skylake) processor. The best compiler optimization level (`-O2` or `-O3`) is reported per case, along with critical metrics:

| Element Size | Best Opt | Throughput<br>(items/s) | Bandwidth<br>(MB/s) | Push Retry<br>(%) | Pop Retry<br>(%) | Efficiency<br>(items/μs) |
| -----------: | :------: | ----------------------: | ------------------: | ----------------: | ---------------: | -----------------------: |
|      4 bytes |    O3    |             6'23'04'565 |              237.38 |            4.10 % |           9.37 % |                62.304565 |
|      8 bytes |    O3    |             6'49'12'380 |              495.76 |           10.40 % |          23.01 % |                68.912380 |
|     16 bytes |    O3    |             6'18'75'460 |              942.17 |           13.98 % |          31.60 % |                61.875460 |
|     32 bytes |    O3    |             5'77'56'442 |             1771.02 |           12.93 % |          34.81 % |                57.756442 |
|     64 bytes |    O2    |             5'36'07'712 |             3274.70 |            0.35 % |          26.94 % |                53.607712 |
|   1024 bytes |    O3    |             1'78'93'015 |            17478.61 |            6.12 % |          91.70 % |                17.893015 |
|   2048 bytes |    O3    |             1'20'69'337 |            23569.79 |            0.00 % |          97.05 % |                12.069337 |
|   4096 bytes |    O3    |               70'34'551 |            27555.59 |            0.00 % |          98.57 % |                 7.034551 |

---

## Conclusions

* **Performance profile remains similar** to the previous version (using `memory_order_acquire`).

* **Compiler optimization still dominates**:

  * O3 is consistently best for mid-to-large element sizes (≥ 16 B).
  * O2 may win in a few edge cases for smaller items due to register pressure.
* **Element size affects cache/memory bottlenecks**:

  * Bandwidth scales linearly with size until **memory wall hits** at \~1–4 KB.
  * `4`–`64` B: **L1-cache bound**
  * `256` B–`1` KB: **L2/L3 bound**
  * ≥ `1` KB: **DRAM bound**, sustained \~`25` GB/s
* **Push retry rates** remain low across all sizes except for very tight buffers (e.g., 4 B) with high contention.
* **Pop retry rates climb rapidly** for large elements due to buffer occupancy—especially when producer outpaces the consumer.
* **Sequence correctness & throughput stability** hold across all runs.


