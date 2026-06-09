# Layouts

A layout determines how a message is stored in the buffer. It changes only the physical representation, leaving the schema and the generated interfaces unchanged. Each layout strikes a different balance between performance, size, and flexibility.

Each message has its own layout. By default it uses the dynamic layout, which picks its representation at runtime, but you can pin a message to a static layout instead.

## Static and Dynamic Dispatch

YaFF dispatches reads in one of two modes, depending on when a message's layout is resolved:

- **Static dispatch.** The layout is fixed at compile time, so the generated code reads a field directly, with no runtime decision. It is the cheapest read path. You opt into it by pinning a message to a layout via options (see [Controlling Layouts Statically](overview.md#controlling-layouts-statically)).
- **Dynamic dispatch** (the default). The layout is decided at runtime, which keeps a message adaptable and its schema free to evolve. Its dispatch is built to be free for the fastest layout and to add only a small overhead in slower cases.

Moving a message between static and dynamic dispatch is a conditionally safe migration (see [Changing Message Layout](evolution.md#changing-message-layout)).

## Fixed Layout

**Fixed Layout** stores a message as a plain packed struct with a fixed size and no header. Reading a field is a single direct memory access, exactly like reading a native C++ struct, and because the size never changes a fixed message can be inlined into other structures (for example, arrays). The trade-off is a frozen schema: you can't add or remove fields. It honors default values but works with [implicit presence](overview.md#field-presence) only.

**Key trade-off:** delivers maximum execution speed and zero metadata overhead at the cost of a completely frozen schema that cannot evolve.

## Flat Layout

**Flat Layout** is a packed struct with a small two-byte header, which adds the ability to evolve the schema. Reading a field costs one extra read and a branch compared to the fixed layout, keeping performance very close to a direct struct access. It supports default values and both implicit and explicit presence, sizing its metadata to what each message needs.

Because each field's position is computed from the sizes of the fields before it, the flat layout has to know the size of every slot. You can still add or remove fields, but a removed field must keep its type recorded in the schema to preserve physical alignment (see [Preserving Types for Reserved](overview.md#preserving-types-for-reserved)).

**Key trade-off:** provides near-native performance and semantic features, but incurs a size overhead for sparse data and requires type preservation during schema mutations.

## Sparse Layout

**Sparse Layout** addresses fields through a meta table rather than fixed physical positions. Reading a field requires four memory reads and two branches, which is exactly twice the read cost of the flat layout. The layout incurs a six-byte overhead per message, plus one or two bytes in the table for each field number up to the highest one used (gaps in the id range are counted too), but the payload itself only consumes space for the fields that are actually set.

Because it decouples a field's number from its position, the sparse layout supports flexible schema mutation. You can add or remove fields without constraints on typing or position, making gaps or scattered field numbers perfectly safe. It also supports default values, as well as both implicit and explicit presence.

**Key trade-off:** delivers a compact representation for sparse data and schema mutation flexibility, but incurs a performance overhead and a larger size overhead for dense datasets.

## Dynamic Layout

**Dynamic Layout** is the default configuration for all messages. Instead of locking a message into a single physical representation at compile time, it uses dynamic dispatch to select the optimal underlying layout. Reading a field is optimized: the dispatch mechanism is designed to add zero overhead for the fastest layout paths and only a minor penalty in slower cases.

Currently, the dynamic layout operates by switching between the Flat and Sparse layouts. It uses the efficient Flat layout as long as the schema's structure permits (meaning there are no untyped gaps or scattered field numbers). If the schema evolves in a way that breaks flat alignment, such as removing a field without explicitly preserving its type, the runtime automatically transitions to the Sparse layout. By combining both strategies, the dynamic layout supports all semantic features while providing the unrestricted schema mutation flexibility of a purely sparse representation.

**Key trade-off:** combines the core strengths of the Flat and Sparse layouts, at the cost of a slight, variable dispatch overhead compared to statically pinned layouts.

## Layout Comparison

The following table summarizes the performance characteristics, memory overhead, and feature support for each layout. Use it as a quick reference to understand the physical and semantic trade-offs at a glance.

| Layout | Read Access | Per Message Overhead | Per Field Overhead | Schema Evolution | Semantic Features |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Fixed** | 1 read, 0 branches | 0 bytes | 0 bytes | **Frozen:** Cannot add or remove fields. | Defaults, Implicit presence only, Not self-describing |
| **Flat** | 2 reads, 1 branch | 2 bytes | 0 bytes (Implicit), 1 bit (Explicit) | **Restricted:** Requires type preservation for removed fields. | Defaults, Implicit and Explicit presence, Not self-describing |
| **Sparse** | 4 reads, 2 branches | 6 bytes | 1–2 bytes based on field number | **Unrestricted:** Scattered field numbers and gaps are safe. | Defaults, Implicit and Explicit presence, Self-describing |
| **Dynamic** | 2 reads, 1 branch (Flat) and 4 reads, 3 branches (Sparse) | 2 bytes (Flat), 6 bytes (Sparse) | 2 bits (Flat Implicit), 3 bits (Flat Explicit), 1-2 bytes (Sparse) | **Unrestricted:** Automatically switches representation to support gaps. | Defaults, Implicit and Explicit presence, Self-describing |

## Choosing a Layout

As a rule of thumb, **keep the default Dynamic layout**. It provides the best balance of speed and structural flexibility for general application logic without requiring manual maintenance.

Opt into a static layout only if you need to optimize for specific constraints:

* **Choose Fixed layout** if the schema is permanently frozen and the message represents a small primitive (like a coordinate pair or key/value entry) that needs to be inlined into arrays.
* **Choose Flat layout** if the data is dense, maximum performance is critical, and you can commit to preserving field types manually during schema evolution.
* **Choose Sparse layout** if the schema naturally contains gaps or scattered field numbers, and you want to completely avoid the dynamic layout's runtime dispatch check.
