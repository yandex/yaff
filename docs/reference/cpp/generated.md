# C++ Generated Code

This page describes the C++ code that the YaFF compiler produces from a `.proto` file, and how it relates to the original Protobuf interfaces. It assumes you are familiar with the [Schema Overview](../../schema/overview.md).

The generated code splits into two parts:

- **Serialization and deserialization.** Serialization turns a Protobuf message into a YaFF representation; deserialization reconstructs the Protobuf message from it.
- **Zero-copy reading.** A proto-like interface reads fields directly from the representation, with no parsing step. This is where YaFF's performance comes from.

The reading interface is immutable. Because YaFF is an alternative wire format, you modify a message through Protobuf: deserialize the representation, change the Protobuf message, and serialize it again.

## Packages

A Protobuf `package` becomes a nested C++ namespace under the generation root. With the default root `protoyaff`:

```proto
package foo.bar;
```

```cpp
namespace protoyaff::foo::bar { ... }
```

The root namespace is configurable: pass the `namespace` parameter to the plugin to replace `protoyaff` with your own (see the [Integration Guide](../../integration.md)).

This outer namespace exists to avoid collisions: the Protobuf compiler generates its own types for the same `.proto` directly under the package namespace (`foo::bar`), so the `protoyaff` root keeps the YaFF types apart from them.

## Messages

Every message in your schema generates a read-only view type of the same name, along with functions to serialize a message into a YaFF representation and deserialize it back into Protobuf.

For the minimal declaration:

```proto
message Foo {}
```

the compiler generates `Foo`, which privately derives from an internal zero-copy base. Reading a field through it compiles down to a direct memory access with no parsing step.

Unlike a Protobuf message, a `Foo` view is immutable and does not own its data: it is a view onto an existing YaFF representation. You obtain one by reading a serialized message, and you never construct, copy, or subclass it yourself. The view's physical [layout](../../schema/layouts.md) is an implementation detail and does not affect its interface.

Besides its field accessors, the `Foo` view exposes a few member types:

- `MetaType`: the message's static layout metadata, used internally by the accessors.
- `ProtobufType`: the corresponding Protobuf message type, used when serializing from or deserializing to Protobuf.
- `enum MessageIds`: each field's YaFF identifier as a named constant (`ID_<FIELD>`).

and a couple of functions, each with a free-function analog:

- `static const Foo& Default()` (free: `FooDefault()`): a const instance with all fields unset.
- `static const ::yaff::reflect::MessageDescriptor* Descriptor()` (free: `FooDescriptor()`): the type's descriptor, describing the message's fields and their types for runtime inspection.

### Serializing

The simplest way to produce a representation is to convert an existing Protobuf message with `yaff::Serialize`, which returns a buffer:

```cpp
const auto buffer = yaff::Serialize<Foo>(proto);
// buffer.Data() and buffer.Size() give the serialized bytes
```

YaFF also generates a lower-level, FlatBuffers-style [interface](#serializing-messages-directly) for writing fields straight into the buffer. Reach for it when you want maximum performance and don't need a Protobuf object.

### Zero-Copy Reading

Reading a message through its view, without converting it back to Protobuf, is the primary way to access YaFF data. A serialized message is read with `yaff::ReadMessage`:

```cpp
const Foo& view = yaff::ReadMessage<Foo>(data);
```

Here `data` points to the start of a serialized representation, for example `buffer.Data()` from above, bytes received over the network, or a memory-mapped file. The call neither parses nor copies: it returns a view that overlays those bytes in place, which is what makes reads zero-copy. A null `data` pointer yields `Foo::Default()`.

`ReadMessage` performs no validation and trusts the bytes it is given; reading a malformed or untrusted buffer is undefined behavior (see [Untrusted Messages](untrusted.md)).

### Deserializing

A serialized message is turned back into a Protobuf message with `yaff::ParseMessage`, or written into an existing one with `ParseTo`:

```cpp
auto proto = yaff::ParseMessage<Foo>(data);  // a fresh Protobuf message
view.ParseTo(proto);                         // or fill an existing one
```

Deserialization is lossless, with one exception: unknown fields.

#### Unknown Fields

Unknown fields are well-formed fields present in the serialized data but absent from the schema used to read it, for example a field added by a newer binary and read by an older one (see the [Protobuf guide](https://protobuf.dev/programming-guides/proto3/#unknowns)).

When you deserialize into a Protobuf message, YaFF does not carry these fields across, because reconstructing them on the Protobuf side would be expensive. This differs from Protobuf, which preserves unknown fields through a parse and serialize round trip.

The loss only happens when you materialize a Protobuf object. As long as you use YaFF as a zero-copy representation, reading fields directly or forwarding the serialized bytes onward, every field is preserved, known or not, since it never leaves the buffer.

### Nested Types

A nested message is flattened into the enclosing namespace with an underscore-joined name:

```proto
message Foo {
    message Bar { ... }
}
```

```cpp
struct Foo_Bar final { ... };  // protoyaff::Foo_Bar
```

The same rule applies to nested enums, and deeper nesting joins every level with underscores (`Foo_Bar_Baz`).

## Fields

The accessors below are how you read a field during [Zero-Copy Reading](#zero-copy-reading). Each one reads directly from the underlying memory with no parsing, behind a proto-like interface. What is generated depends on the field's type and [presence](../../schema/overview.md#field-presence).

### Explicit Presence Numeric Fields

For a numeric field with explicit presence:

```proto
optional int32 foo = 1;
```

the compiler generates the following accessor methods:

- `bool has_foo() const`: Returns `true` if the field is set.
- `int32_t foo() const`: Returns the current value of the field. If the field is not set, returns the default value.

For other numeric field types `int32_t` is replaced with the corresponding C++ type.

### Implicit Presence Numeric Fields

For a numeric field with implicit presence:

```proto
int32 foo = 1;
```

the compiler generates the following accessor method:

- `int32_t foo() const`: Returns the current value of the field. If the field is not set, returns the default value.

There is no `has_` accessor: with implicit presence, a field equal to the default cannot be told apart from an unset one. For other numeric field types `int32_t` is replaced with the corresponding C++ type.

### Explicit Presence String/Bytes Fields

For a `string` or `bytes` field with explicit presence:

```proto
optional string foo = 1;  // or: optional bytes foo = 1;
```

the compiler generates the following accessor methods:

- `bool has_foo() const`: Returns true if the field is set.
- `const ::yaff::String& foo() const`: Returns the current value of the field as a zero-copy view. If the field is not set, returns the default value (an empty string).

`yaff::String` is a read-only view over the bytes in the representation. It converts implicitly to `std::string_view` (or call `AsStringView()` explicitly), compares directly against string literals and `std::string`, and streams to `std::ostream`. Both `string` and `bytes` map to this type.

### Implicit Presence String/Bytes Fields

For a `string` or `bytes` field with implicit presence:

```proto
string foo = 1;  // or: bytes foo = 1;
```

the compiler generates the following accessor method:

- `const ::yaff::String& foo() const`: Returns the current value of the field as a zero-copy view. If the field is not set, returns an empty string.

There is no `has_` accessor: with implicit presence, an empty value cannot be told apart from an unset one. The returned `yaff::String` behaves as described under [Explicit Presence String/Bytes Fields](#explicit-presence-stringbytes-fields).

### Explicit Presence Enum Fields

Given the enum type:

```proto
enum Bar {
  BAR_UNSPECIFIED = 0;
  BAR_VALUE = 1;
  BAR_OTHER_VALUE = 2;
}
```

For a field of this type with explicit presence:

```proto
optional Bar bar = 1;
```

the compiler generates the following accessor methods:

- `bool has_bar() const`: Returns true if the field is set.
- `Bar bar() const`: Returns the current value of the field. If the field is not set, returns the default value.

The value is stored as an `int32`. Because YaFF enums are always open, an unrecognized numeric value is returned as is rather than dropped (see [Enum Types](../../schema/overview.md#enum-types)).

### Implicit Presence Enum Fields

Using [the same](#explicit-presence-enum-fields) `Bar` enum, for a field with implicit presence:

```proto
Bar bar = 1;
```

the compiler generates the following accessor method:

- `Bar bar() const`: Returns the current value of the field. If the field is not set, returns the default value.

There is no `has_` accessor: with implicit presence, the default value cannot be told apart from an unset field. As with explicit enums, the value is stored as an `int32` and the enum is open.

### Explicit Presence Embedded Message Fields

Given the message type:

```proto
message Bar {}
```

For a field of this type (message fields always have explicit presence):

```proto
Bar bar = 1;
```

the compiler generates the following accessor methods:

- `bool has_bar() const`: Returns true if the field is set.
- `const Bar& bar() const`: Returns the current value of the field. If the field is not set, returns `Bar::Default()` (a `Bar` with all fields unset).

The returned `Bar` is itself a read-only view, so reading a nested message stays zero-copy.

### Repeated Numeric Fields

For a repeated numeric field:

```proto
repeated int32 foo = 1;
```

the compiler generates the following accessor methods:

- `int foo_size() const`: Returns the number of elements currently in the field.
- `int32_t foo(int index) const`: Returns the element at the given zero-based index. Calling this method with an index outside `[0, foo_size())` yields undefined behavior.
- `const ::yaff::Array<int32_t>& foo() const`: Returns a view over the field's elements.

`yaff::Array<T>` is a read-only view over the elements, with an STL-like interface: `size()` and `empty()`, indexed access through `operator[]` and `Get(i)`, and `begin()`/`end()` iterators that work with range-based `for` and standard algorithms. Element access reads straight from the representation, so iterating copies nothing.

For other numeric field types `int32_t` is replaced with the corresponding C++ type.

### Repeated String Fields

For a repeated `string` or `bytes` field:

```proto
repeated string foo = 1;  // or: repeated bytes foo = 1;
```

the compiler generates the following accessor methods:

- `int foo_size() const`: Returns the number of elements currently in the field.
- `const ::yaff::String& foo(int index) const`: Returns the element at the given zero-based index. Calling this method with an index outside `[0, foo_size())` yields undefined behavior.
- `const ::yaff::Array<...>& foo() const`: Returns a view over the field's elements.

Each element is a `yaff::String` (see [String and Bytes Fields](#explicit-presence-stringbytes-fields)), and the container behaves like the `yaff::Array` described under [Repeated Numeric Fields](#repeated-numeric-fields).

### Repeated Enum Fields

Using [the same](#explicit-presence-enum-fields) `Bar` enum, for a repeated field:

```proto
repeated Bar bar = 1;
```

the compiler generates the following accessor methods:

- `int bar_size() const`: Returns the number of elements currently in the field.
- `Bar bar(int index) const`: Returns the element at the given zero-based index. Calling this method with an index outside `[0, bar_size())` yields undefined behavior.
- `const ::yaff::Array<...>& bar() const`: Returns a view over the field's elements.

Elements are stored as `int32` and read back as `Bar`. The container behaves like the `yaff::Array` described under [Repeated Numeric Fields](#repeated-numeric-fields).

### Repeated Embedded Message Fields

Using [the same](#explicit-presence-embedded-message-fields) `Bar` message, for a repeated field:

```proto
repeated Bar bar = 1;
```

the compiler generates the following accessor methods:

- `int bar_size() const`: Returns the number of elements currently in the field.
- `const Bar& bar(int index) const`: Returns the element at the given zero-based index. Calling this method with an index outside `[0, bar_size())` yields undefined behavior.
- `const ::yaff::Array<...>& bar() const`: Returns a view over the field's elements.

Each element is itself a read-only `Bar` view, so iterating stays zero-copy. The container behaves like the `yaff::Array` described under [Repeated Numeric Fields](#repeated-numeric-fields).

### Oneof Fields

For a oneof:

```proto
oneof example_name {
    string foo = 1;
    Bar bar = 2;
}
```

each member generates the same accessors as an explicit-presence field of its type:

- `bool has_<member>() const`: Returns true if `<member>` is the member that is currently set.
- the member's getter (`const ::yaff::String& foo() const`, `const Bar& bar() const`, ...): Returns the value when its member is the one set, otherwise the default.

At most one member is set at a time.

### Map Fields

A `map` is stored as an array of key/value entries kept sorted by key, not as a hash table. For the field definition:

```proto
map<int32, int32> weight = 1;
```

the compiler generates a key/value entry message, with `key()` and `value()` accessors matching the map's types:

```cpp
struct <Message>_WeightEntry {
    int32_t key() const;
    int32_t value() const;
};
```

and a single field accessor:

- `const ::yaff::Array<...>& weight() const`: Returns a view over the entries, sorted by key.

The container is the same `yaff::Array` described under [Repeated Numeric Fields](#repeated-numeric-fields): you iterate entries in key order and look them up by key with `find()`, `contains()`, and `equal_range()`, all backed by binary search.

## Oneof

Given a oneof definition:

```proto
oneof example_name {
    int32 foo_int = 4;
    string foo_string = 9;
}
```

the compiler generates the following C++ enum type:

```cpp
enum ExampleNameCase : ::yaff::FieldId {
    kFooInt = 4,
    kFooString = 9,
    EXAMPLE_NAME_NOT_SET = 0,
};
```

and the accessor:

- `ExampleNameCase example_name_case() const`: Returns the enum value indicating which member is set, or `EXAMPLE_NAME_NOT_SET` if none of them is set.

## Enumerations

Given an enum definition:

```proto
enum Foo {
  VALUE_A = 0;
  VALUE_B = 5;
  VALUE_C = 1234;
}
```

the compiler generates a scoped C++ enum, `enum class Foo : int32_t`, with the same set of values (accessed as `Foo::VALUE_A`). It also generates the following functions:

- `bool Foo_IsValid(int value)`: Returns true if the numeric value matches one of `Foo`'s defined values (`0`, `5`, or `1234` above).
- `std::string_view Foo_Name(Foo value)`: Returns the name for the given value, or an empty string if it has none. If several names share a number, the first one defined is returned.
- `bool Foo_Parse(std::string_view name, Foo* value)`: If `name` is a valid value name, assigns it into `*value` and returns true; otherwise returns false.
- `Foo Foo_MIN`: the smallest defined value (`VALUE_A` above).
- `Foo Foo_MAX`: the largest defined value (`VALUE_C` above).
- `int Foo_ARRAYSIZE`: always `Foo_MAX + 1`.

All YaFF enums are open: an unrecognized numeric value is preserved when reading, not dropped, and is returned as is by enum field accessors. `Foo_IsValid` is therefore informational, and any `int32` is safe to cast to `Foo`. For the same reason, a `switch` over an enum without a `default` case cannot catch every value even when all named values are listed; always include a `default`, or guard with `Foo_IsValid`, to handle unknown values.

An enum nested in a message is flattened to `Message_Foo`, as described under [Nested Types](#nested-types).

## Any

> **Work in progress.** A native, zero-copy representation of `Any` (and the other well-known types) is planned. For now YaFF treats them as ordinary messages, as described below.

Given an `Any` field:

```proto
import "google/protobuf/any.proto";

message ErrorStatus {
  string message = 1;
  google.protobuf.Any details = 2;
}
```

YaFF has no special handling for `google.protobuf.Any`, so `details()` returns a plain read-only view over its two underlying fields, with no packing helpers:

- `const ::yaff::String& type_url() const`: the type URL identifying the stored message.
- `const ::yaff::String& value() const`: the serialized payload, which YaFF keeps as an opaque blob.

To pack or unpack the payload, deserialize into a Protobuf `Any` and use its `PackFrom`, `UnpackTo`, and `Is<T>()` methods.

## Serializing Messages Directly

[`yaff::Serialize`](#serializing) is the convenient path when you already have a Protobuf message. To build a representation without one, for maximum performance, use the generated `Serialize<Message>` functions together with a `yaff::Serializer` directly. This is the lower-level, FlatBuffers-style interface.

> **Note:** This is an internal API and is not recommended for general use. Its arguments are positional, so removing a field from the middle of a message can silently shift the remaining arguments onto the wrong fields whenever their types happen to match, with no compile error to catch it. Unless you specifically need this path, serialize from a Protobuf message instead.

For each message, the compiler generates a free function that takes the fields directly, all defaulted:

```cpp
::yaff::InternalOffset<Author> SerializeAuthor(
    ::yaff::Serializer& ys,
    uint64_t a_id = 0,
    ::yaff::InternalOffset<::yaff::String> a_name = 0,
    std::optional<bool> a_verified = std::nullopt);
```

The argument type follows the field:

- implicit scalar or enum: passed by value (`uint64_t a_id`).
- explicit scalar or enum: a `std::optional` (`std::optional<bool> a_verified`); leave it `std::nullopt` to keep the field unset.
- `string` or `bytes`: an `::yaff::InternalOffset<::yaff::String>`, produced with `ys.SerializeString(...)`.
- nested message: an `::yaff::InternalOffset<Bar>`, produced with `SerializeBar(ys, ...)`.
- repeated or map field: an `::yaff::InternalOffset<::yaff::Array<...>>`, produced with `ys.SerializeArray(...)`.

A zero offset, the default, leaves the field unset.

Strings, arrays, and nested messages must be serialized first, into offsets, and only then passed to the enclosing message: the buffer is built back to front, so inner objects are written before the message that refers to them. The main producers are:

- `ys.SerializeString(std::string_view)`, returning `InternalOffset<String>`.
- `ys.SerializeArray(const std::vector<T>&)`, or `ys.SerializeArray(size_t len, gen)` where `gen(i)` returns element `i`, returning `InternalOffset<Array<...>>`.
- `SerializeBar(ys, ...)` for a nested message, returning `InternalOffset<Bar>`.

Once the top-level message offset is built, finish the serializer and take the buffer:

```cpp
yaff::Serializer ys;
const auto name = ys.SerializeString("Alice");
const auto author = SerializeAuthor(ys, /* id */ 42, name, /* verified */ true);
ys.Finish(author);
const auto buffer = ys.Release();  // buffer.Data() / buffer.Size()
```

A single `Serializer` can be reused across messages by calling `Reset()` between them; its full interface is covered in the YaFF library reference.
