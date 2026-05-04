# Support Library

The support library is YaFF's hand-written, header-only runtime: the headers under `yaff/` that the generated code includes and builds on. It provides the serializer, the entry points for reading and converting messages, the view value types (`String`, `Array`), the message layout types, and lightweight reflection. This page is the reference for those types; for the per-message code the compiler emits, see [C++ Generated Code](generated.md).

## Serializer

`yaff::Serializer` writes serialized data straight into an internal buffer as you go, with no intermediate object tree: each `Serialize*` call appends bytes and hands back an offset to them. The buffer is built back to front, so inner values are written before the messages that refer to them (see [Serializing Messages Directly](generated.md#serializing-messages-directly)).

By default the serializer deduplicates identical flat byte sequences, so repeated content is stored once: scalar arrays, strings, and the meta tables of sparse messages.

### Methods

Serializing:

- `InternalOffset<String> SerializeString(std::string_view str)`: Serializes a `string` or `bytes` value and returns an offset to it. Deduplicated.
- `InternalOffset<Array<T>> SerializeArray(...)`: Serializes a repeated field. Overloads accept a `std::vector<T>`, a pointer and length, or a length and a generator `gen(i)` returning element `i`. Scalar arrays are deduplicated; offset arrays are not.

Building a message:

A message is opened, its fields are added, and it is closed, returning an offset:

- `void StartFixedMessage<M>()` / `Offset FinishFixedMessage()`: a fixed-layout message. `M` is the message's `MetaType`.
- `void StartFlatMessage<M>(bool implicit = false, bool sized = false)` / `Offset FinishFlatMessage()`: a flat-layout message. `implicit` marks a message that uses implicit presence only; `sized` writes the field-size metadata needed when flat runs as a dynamic alternative.
- `void StartSparseMessage(bool implicit = false)` / `Offset FinishSparseMessage()`: a sparse-layout message.

There is no dynamic primitive: a dynamic message is written as one of these representations.

Between start and finish, add each set field with `AddField`:

- `void AddField<T>(FieldId id, T value, T def)`: a scalar field, where `def` is the field's default.
- `void AddField<T>(FieldId id, InternalOffset<T> offset)`: a reference field (string, message, or array).

`AddField` must be called in descending id order, the reverse of the fields' schema numbers, because the buffer is built back to front; calling it in any other order is invalid.

Whether a call marks the field as present depends on the `implicit` flag the message was started with:

- **Explicit** (`implicit = false`): every `AddField` records the field as set, whatever its value.
- **Implicit** (`implicit = true`): a scalar whose `value` equals `def` is skipped, so the field reads back as unset.

In both modes the reference overload skips a field whose `offset` is zero. Each `Finish*Message` returns an `Offset`, which you wrap in an `InternalOffset<Message>` to nest the message or to finish the buffer.

Finishing:

- `void Finish(InternalOffset<T> root)`: Marks `root` as the top-level message of the buffer. Call it once, after the top-level message is built.
- `DetachedSegment Release()`: Returns the finished, self-owned buffer and resets the serializer. The buffer exposes `Data()` and `Size()`.
- `const std::byte* Data() const` / `size_t Size() const`: Access the finished bytes in place, without detaching them.

### Choosing the Dynamic Representation

When a message uses the dynamic [layout](../../schema/layouts.md), it is written as either the flat or the sparse representation, and how that is chosen depends on how you serialize.

A generated `Serialize<Message>` function takes the representation from the serializer's current setting, which therefore acts as context for the generated code:

- `void EnforceDynamicAlternative(MessageLayout layout)`: Sets the representation (`MESSAGE_LAYOUT_FLAT` or `MESSAGE_LAYOUT_SPARSE`) used for dynamic messages serialized afterward. The default is flat.
- `MessageLayout GetForcedDynamicAlternative() const`: Returns the current setting.

When [building a message by hand](#methods), this setting plays no role: you pick the representation directly by calling `StartFlatMessage` or `StartSparseMessage`.

Automatic, data-driven selection and a more convenient interface for the generated path are planned; until then the representation is chosen explicitly.

## Message

The support library defines the layout view types that underlie a generated message: `FixedMessage<M>`, `FlatMessage<M>`, `SparseMessage`, and `DynamicMessage<M>`, where `M` is the message's `MetaType`. The generated message privately derives from one of them, and its typed accessors are thin wrappers over their uniform low-level read API. The same three primitives work across every layout:

- `T ReadValue<T>(FieldId id, T defaultVal) const`: Reads a scalar or enum field by id, returning `defaultVal` if it is unset. Always pass the field's own default.
- `const T* ReadLayout<T>(FieldId id, const T* defaultPtr = nullptr) const`: Reads a reference field (string, message, or array) by id, returning `defaultPtr` if it is unset.
- `bool ReadPresence<T>(FieldId id) const`: Returns whether the field is set.

`MetaType` is the static layout metadata that a fixed, flat, or dynamic view needs (the sparse layout uses none). It is a struct of `static constexpr` members:

- `FLAT_OFFSETS`: a `std::array<FieldOffset, N>` giving each field's inline offset, indexed by id minus one.
- `STATIC_FLAGS`: a `std::array<bool, N>` marking which field offsets are known statically and which are corrected at runtime to account for gaps.
- `DELETED_IDS`: a `std::array<FieldId, K>` of removed field ids, used for that gap correction.
- `LIMIT` (fixed layout only): the message's fixed size.

Taken together, the layout views and a hand-written `MetaType` let you read messages without any generated code, and the [serializer](#serializer) primitives write them the same way. This is handy where declaring and generating a full schema would be unnecessary, such as in tests.

## Reflection

YaFF reuses your Protobuf schema directly, so full schema reflection naturally belongs on the Protobuf side. If you need rich, descriptor-based introspection, use Protobuf's own reflection on the message's `ProtobufType`.

For compact tooling built directly on top of YaFF, pulling in the full Protobuf reflection stack is often too heavy. For that, YaFF ships lightweight reflection of its own, similar in spirit to FlatBuffers' mini-reflection: just enough metadata to inspect and walk a message generically, with no dependency on Protobuf.

### Descriptors

Each generated message and enum exposes a lightweight descriptor:

- `Foo::Descriptor()` and the free `FooDescriptor()` return a `const ::yaff::reflect::MessageDescriptor*`.
- An enum's free `FooDescriptor()` returns a `const ::yaff::reflect::EnumDescriptor*`.

The descriptors form a small tree of plain structs (`MessageDescriptor` holding `FieldDescriptor`s, each pointing at a `TypeDescriptor`, alongside `EnumDescriptor` and `EnumValueDescriptor`), generated as `constexpr` data embedded directly in the code. For the exact fields of each struct, see [reflect.h](https://github.com/yandex/yaff/blob/main/include/yaff/reflect.h).

### Walking a Message

To traverse a message without knowing its type at compile time, implement the `::yaff::reflect::AbstractVisitor` callback interface and drive it with `::yaff::reflect::Visitor`:

```cpp
class MyVisitor : public ::yaff::reflect::AbstractVisitor {
    void OnMessageStart() override;  void OnMessageEnd() override;
    void OnArrayStart() override;    void OnArrayEnd() override;
    void OnField(const char* name) override;
    void OnElement(size_t i) override;
    void OnString(std::string_view v) override;
    void OnInt32(int32_t v) override;
    // ... OnBool / OnUint32 / OnInt64 / OnUint64 / OnFloat / OnDouble / OnEnum
};

MyVisitor sink;
// messagePtr points to the message inside a YaFF representation
::yaff::reflect::Visitor(&sink).VisitMessage(messagePtr, Foo::Descriptor());
```

The visitor walks the message in field order, emitting a callback per field, element, and scalar, and skips deprecated fields. That is enough to build generic tooling on top of a YaFF representation, such as a debug dump or a custom text or JSON formatter, with no type-specific code.
