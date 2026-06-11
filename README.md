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
note: stress test (src/stress_test_queue.cpp and src/stress_test_stack.cpp) written by Claude Opus 4.6 (Thinking)

```bash
# build correctness stress test with thread sanitizer to track and check 
# concurrent memory accesses
# note: if issue with ASLR (adress space layout randomization), such as 
#       mmap_rnd_bits > 28, need to set it to <= 28
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TSAN=ON -B build_tsan
cmake --build build_tsan --target stress_test_stack stress_test_queue
./build_tsan/stress_test_stack
./build_tsan/stress_test_queue
```
## build benchmark performance for comparison
note: benchmark (src/bench.cpp) written by Claude Opus 4.6 (Thinking)

```bash
# performance benchmark
cmake -DCMAKE_BUILD_TYPE=Release -B build_release
cmake --build build_release --target bench
./build_release/bench              # default: 200K ops/thread
./build_release/bench 500000       # custom ops count
```

### results and interpretation of benchmark
todo
