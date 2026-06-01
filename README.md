# YaFF [Yet another Flat Format]

[![CI](https://github.com/yandex/yaff/actions/workflows/ci.yml/badge.svg)](https://github.com/yandex/yaff/actions/workflows/ci.yml)

YaFF is a high-performance C++ serialization library that provides a zero-copy wire format for the [Protobuf](https://protobuf.dev/) ecosystem. While the `.proto` schema remains your single source of truth, YaFF changes the physical representation of data to eliminate runtime overhead. This provides performance-critical applications with direct, zero-parsing access through a proto-like interface, while allowing less performance-sensitive components to parse the wire format back into Protobuf messages.

YaFF shares the zero-copy foundations of [FlatBuffers](https://flatbuffers.dev/), but concentrates on server-side runtimes and adaptive layout strategies that bring read performance close to native C++ structs, preserving interoperability with Protobuf.

## Key Features

* **Zero-Copy Speed** — Eliminates parsing entirely with mmap-compatible layouts.
* **Adaptive Layouts** — Allows explicit configuration or runtime adjustment to manage tradeoffs between speed, memory overhead, and schema flexibility.
* **Protobuf Native** — Uses existing `.proto` files and respects Protobuf semantics.
* **Incremental Adoption** — Emulates standard proto interfaces and provides two-way message conversion for easy module-by-module integration.

## Quick start

TBD

## Roadmap

YaFF’s core design decouples schema definitions from the underlying physical memory layout. This separation allows the exact same data structures to support multiple runtime representations, enabling optimization for specific access patterns without changing the source schema.

YaFF is under active development. Planned and in-progress work includes:

* **Columnar Layout** — Compact representation for large repeated fields, optimized for analytics and ML pipelines.
* **Automated Adaptivity** — Layout selection driven automatically by data analysis or via profile-guided modes.
* **Multi-Language Bindings** — Extending zero-copy data access to languages beyond C++.

## Integration Guide

YaFF can be added to a project in two ways:

- **CMake** — build and install YaFF, then use `find_package`
- **Conan** — build YaFF into the local Conan cache, then declare it as a dependency

Both methods expose the same set of CMake targets and the `yaff_generate()` helper function.

### Available CMake Targets

| Target | Type | When to use |
|---|---|---|
| `yaff::core` | `INTERFACE` library | Always — provides all runtime headers (`include/yaff/*.h`) |
| `yaff::proto` | `STATIC` library | When reading/writing Protobuf-backed YaFF messages |
| `yaff::protoc_plugin` | Executable | Used internally by `yaff_generate()`; not linked by consumers |

Most applications only need `yaff::core` and `yaff::proto`.

For step-by-step integration instructions see [docs/integration.md](docs/integration.md).

Working consumer projects for both paths are in [integration_example/](integration_example/).

## Contributing

Contributions are welcome! See the [contributor's guide](CONTRIBUTING.md) for how to report issues and submit pull requests.

## License

This project is licensed under the Apache 2.0 License (Apache-2.0). [Apache License 2.0](LICENSE).
