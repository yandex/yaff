# Benchmarks

One of YaFF's features is its support for multiple in-memory representations: a choice of layout strategies, with static or dynamic switching between them. Each layout makes its own tradeoff between read speed, serialized size, and schema flexibility (see [Layouts](../schema/layouts.md)). These benchmarks put numbers on those tradeoffs, and measure how YaFF compares to Protobuf, FlatBuffers, and raw C++ structs on identical workloads.

The suite covers two axes:

- [Read Access](access.md) — the cost of reading fields out of a serialized buffer, for both flat messages and a chain of nested messages.
- [Serialized Size](space.md) — how many bytes a message occupies on the wire, across a range of field counts and population densities.

## Environment

The benchmarks are built with [google/benchmark](https://github.com/google/benchmark) in a Release build. Measurements were taken on the following configurations:

| Compiler | CPU | Architecture |
| :--- | :--- | :--- |
| Clang 20.1.8 | AMD EPYC 7713 | x86-64 |

Absolute benchmark values depend on the host's CPU and memory (for example, EPYC 7713 uses DDR4). The relative ratios between the formats are expected to hold across hardware, since they follow from each layout's structural access cost rather than from any property of the machine.

## Building and Running

Benchmarks are off by default. They depend on [google/benchmark](https://github.com/google/benchmark) and [FlatBuffers](https://github.com/google/flatbuffers) (used as comparison baselines), which must be available to CMake. The simplest way to pull them in is Conan, which fetches both dependencies for you when benchmarks are enabled:

```bash
conan install . --build=missing -s build_type=Release -o build_benchmarks=True
cmake --preset conan-release -DYAFF_BUILD_BENCHMARKS=ON
cmake --build --preset conan-release
```

If you provide benchmark and flatbuffers yourself, configure with CMake directly:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DYAFF_BUILD_BENCHMARKS=ON
cmake --build build
```

This builds one executable per benchmark in your build directory. Run one directly to get its results.
