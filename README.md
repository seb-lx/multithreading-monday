# multithreading monday
some experiments with multihreading in C++

## notes
notes can be found in docs/

## simple test builds
```bash
mkdir build_debug # or: mkdir build_release
cd build_debug # or: cd build_release

cmake -DCMAKE_BUILD_TYPE=Debug .. # or cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

## build correctness stress test and 
note: stress test (src/stress_test_queue.cpp and src/stress_test_stack.cpp) written in collaboration with Claude Opus 4.6 (Thinking)

```bash
# build correctness stress test with thread sanitizer to track and check 
# concurrent memory accesses
# note: if issue with ASLR (adress space layout randomization), such as 
#       mmap_rnd_bits > 28, need to set it to <= 28 before via
#       "sudo sysctl vm.mmap_rnd_bits=28"
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TSAN=ON -B build_tsan
cmake --build build_tsan --target stress_test_stack stress_test_queue
./build_tsan/stress_test_stack
./build_tsan/stress_test_queue
```
## build benchmark performance for comparison
note: benchmark (src/bench.cpp) written in collaboration with Claude Opus 4.6 (Thinking)

```bash
# performance benchmark
cmake -DCMAKE_BUILD_TYPE=Release -B build_release
cmake --build build_release --target bench
./build_release/bench              # default: 200K ops/thread
./build_release/bench 500000       # custom ops count
```

### results and interpretation of benchmark
todo: current output comparison, add notes for interpretation

```bash
Thread-Safe Data Structure Benchmark
========================================
Hardware concurrency : 16
Ops per thread (int) : 200000
Ops per thread (1KB) : 50000
Runs per config      : 5 (median)

============================================================
  Stack: Push-Only (write contention)
============================================================

  Threads |     TSS v1 (int) |     TSS v1 (1KB)
----------+------------------+-----------------
        1 |    137.52M ops/s |      2.21M ops/s
        2 |     41.87M ops/s |      1.44M ops/s
        4 |     46.05M ops/s |     890.5K ops/s
        8 |     37.29M ops/s |     704.8K ops/s
       16 |     25.92M ops/s |     533.7K ops/s
       32 |     32.83M ops/s |     529.4K ops/s


============================================================
  Stack: Producer-Consumer
============================================================

  Threads |     TSS v1 (int) |     TSS v1 (1KB)
----------+------------------+-----------------
        2 |     55.82M ops/s |      7.64M ops/s
        4 |     68.67M ops/s |      8.04M ops/s
        8 |     49.08M ops/s |      4.71M ops/s
       16 |     25.91M ops/s |      3.28M ops/s
       32 |     25.06M ops/s |      3.09M ops/s


============================================================
  Queue: Push-Only (write contention)
============================================================

  Threads |     TSQ v1 (int) |     TSQ v2 (int) |     TSQ v1 (1KB) |     TSQ v2 (1KB)
----------+------------------+------------------+------------------+-----------------
        1 |    121.28M ops/s |     48.82M ops/s |      2.36M ops/s |      2.39M ops/s
        2 |     37.12M ops/s |     14.92M ops/s |      1.64M ops/s |      4.03M ops/s
        4 |     41.22M ops/s |     16.45M ops/s |     979.9K ops/s |      4.49M ops/s
        8 |     29.77M ops/s |     12.13M ops/s |     677.6K ops/s |      4.68M ops/s
       16 |     19.97M ops/s |      8.69M ops/s |     524.2K ops/s |      4.49M ops/s
       32 |     21.13M ops/s |      9.58M ops/s |     534.1K ops/s |      4.94M ops/s


============================================================
  Queue: Producer-Consumer
============================================================

  Threads |     TSQ v1 (int) |     TSQ v2 (int) |     TSQ v1 (1KB) |     TSQ v2 (1KB)
----------+------------------+------------------+------------------+-----------------
        2 |     54.40M ops/s |     34.83M ops/s |      3.09M ops/s |      4.38M ops/s
        4 |     49.52M ops/s |     18.48M ops/s |      5.38M ops/s |      6.13M ops/s
        8 |     41.29M ops/s |     11.91M ops/s |      5.45M ops/s |      5.32M ops/s
       16 |     22.71M ops/s |      7.69M ops/s |      3.47M ops/s |      4.24M ops/s
       32 |     25.92M ops/s |      7.69M ops/s |      3.62M ops/s |      4.28M ops/s
```