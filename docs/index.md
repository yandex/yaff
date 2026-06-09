# YaFF

YaFF (Yet another Flat Format) is a high-performance C++ serialization library that provides a zero-copy wire format for the [Protobuf](https://protobuf.dev/) ecosystem. While the `.proto` schema remains your single source of truth, YaFF changes the physical representation of data to eliminate runtime overhead. This provides performance-critical applications with direct, zero-parsing access through a proto-like interface, while allowing less performance-sensitive components to parse the wire format back into Protobuf messages.

YaFF shares the zero-copy foundations of [FlatBuffers](https://flatbuffers.dev/), but concentrates on server-side runtimes and adaptive layout strategies that bring read performance close to native C++ structs, preserving interoperability with Protobuf.

## When to Use YaFF

YaFF is a good fit when:

- **You're comfortable with Protobuf and want faster reads.** You stay in the ecosystem you already know instead of adopting a separate one: a `.proto` is your single source of truth, whether you have an existing schema or start a new project, and you can move to YaFF gradually, converting at the edges.
- **You need the lowest read overhead.** In the hottest paths YaFF's adaptive layouts bring field access close to a native C++ struct.
- **You want the usual zero-copy benefits.** No parse or copy on read, cheap serialization straight into the buffer, and data you can read directly from a memory-mapped file.
- **You control both ends of the pipeline.** Producers and consumers live inside the same [trusted system](reference/cpp/untrusted.md).

## Where to Start

- [Quick start](quick_start.md) — turn your first proto message into a zero-copy representation.
- [Schema Overview](schema/overview.md) — basic and advanced `.proto` schema features.
- [API Reference](reference/cpp/generated.md) — the generated accessors, serialization, and reflection.

## Useful Links

- [GitHub repository](https://github.com/yandex/yaff)
