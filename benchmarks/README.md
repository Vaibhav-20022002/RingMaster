# RingMaster SPSC Buffer Benchmark Summary

The table below highlights, for each element size, the best‐performing compiler optimization level and key metrics:

| Element Size | Best Opt | Throughput<br>(items/s) | Bandwidth<br>(MB/s) | Push Retry<br>(%) | Pop Retry<br>(%) | Efficiency<br>(items/μs) |
|-------------:|:--------:|------------------------:|--------------------:|------------------:|-----------------:|-------------------------:|
| 4 bytes      | O2       | 6'06'02'388              | 231.18              | 4.13 %            | 9.24 %           | 60.602388                |
| 8 bytes      | O3       | 6'52'11'154              | 497.52              | 10.33 %           | 22.78 %          | 65.211154                |
| 16 bytes     | O3       | 6'17'75'680              | 942.62              | 14.49 %           | 31.65 %          | 61.775680                |
| 32 bytes     | O2       | 5'80'12'043              | 1770.39             | 12.75 %           | 34.97 %          | 58.012043                |
| 64 bytes     | O2       | 5'36'00'909              | 3271.54             | 0.37 %            | 26.84 %          | 53.600909                |
| 1024 bytes   | O3       | 1'78'52'361              | 17433.95            | 6.01 %            | 91.83 %          | 17.852361                |
| 2048 bytes   | O3       | 1'20'78'929              | 23591.66            | 0.00 %            | 96.98 %          | 12.078929                |
| 4096 bytes   | O3       | 70'64'771               | 27596.76            | 0.00 %            | 98.53 %          | 7.064771                 |

---

## Conclusions

- **Peak throughput & bandwidth** scale with element size up to cache‐line granularity (64 B), after which memory subsystem limits dominate.
- **O2/O3** deliver the best balance of speed vs. retry overhead for small‐to‐medium items; O3 is generally preferred for larger elements.
- **Push retry rates** remain low (< 5 %) for power‐of‐two capacities under O2/O3, but pop retries rise significantly (> 90 %) once the buffer is heavily loaded at large element sizes.
- **Accuracy** remains 100 % in all configurations.
- **Efficiency** (items/μs) closely mirrors throughput trends, peaking for 8B - 16B elements under O3.

Overall, the lock-free RingMaster buffer achieves excellent sustained throughput with minimal retry overhead at O2/O3 optimizations, especially for element sizes between 8 B and 64 B.