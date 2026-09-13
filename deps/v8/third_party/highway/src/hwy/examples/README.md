# Highway Examples

This directory contains examples demonstrating how to use the Highway SIMD library.

## Examples

### `sum_array_simple.cc`
Minimal code demonstrating how to sum an array of floats using Highway SIMD, with a simple scalar fallback for remainders.

### `sum_array_advanced.cc`
Advanced implementation demonstrating:
- Loop unrolling (factor of 4) for better performance.
- Use of `LoadN` with `FirstN` mask for precise remainder handling without scalar fallbacks.
- Runtime checks and validation against a scalar implementation.

## How to Run

### Using Bazel
To run the examples using Bazel:
```bash
#  copybara:strip_begin(internal)
blaze run //third_party/highway:sum_array_simple
blaze run //third_party/highway:sum_array_advanced
#  copybara:strip_end

bazel run //:sum_array_simple
bazel run //:sum_array_advanced
```

### Using CMake
If you are building Highway with CMake (from the root of the highway directory):
```bash
mkdir build && cd build
cmake .. -DHWY_ENABLE_EXAMPLES=ON
make
./examples/sum_array_simple
./examples/sum_array_advanced
```

### Using Clang directly
To compile and run using `clang++` (from the root of the highway directory):
```bash
clang++ -std=c++17 -O3 -I. hwy/examples/sum_array_simple.cc hwy/targets.cc hwy/per_target.cc hwy/print.cc hwy/abort.cc hwy/aligned_allocator.cc -o sum_array_simple

./sum_array_simple
```

### Using GCC directly
To compile and run using `g++` (from the root of the highway directory):
```bash
g++ -std=c++17 -O3 -I. hwy/examples/sum_array_simple.cc hwy/targets.cc hwy/per_target.cc hwy/print.cc hwy/abort.cc hwy/aligned_allocator.cc -o sum_array_simple

./sum_array_simple
```
*Note: `g++` might emit some assembler warnings like `no SFrame FDE emitted`, which are benign and can be ignored.*
