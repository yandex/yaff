# Yet another Flat Format [YaFF]

YaFF is a C++ serialization library for working with structured data in performance-critical applications with deep [Protobuf](https://protobuf.dev/) integration. Think of it as an alternative [Protobuf](https://protobuf.dev/) wire-format with zero-copy data access support.

---

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
