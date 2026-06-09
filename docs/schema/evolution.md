# Schema Evolution

YaFF supports forward and backward compatibility: code built against one version of a schema can read data written by another. Because YaFF reuses your Protobuf schemas, the natural starting point is Protobuf's own evolution rules. Protobuf sorts schema changes into three categories:

- **Safe**: never breaks compatibility.
- **Unsafe**: breaks compatibility.
- **Conditionally safe**: stays compatible, but some values can be lost or truncated.

Because YaFF is an alternative wire format for Protobuf, the safe and unsafe categories carry over unchanged. YaFF splits Protobuf's conditionally safe category in two: changes it keeps conditionally safe, and changes it treats as conditionally unsafe (unsafe by default). Those splits are what this page covers.

## Safe Changes

Safe migrations are the same in YaFF as in Protobuf: you can add fields, remove fields, add enum values, rename anything, and move a single field in or out of a oneof. The default dynamic layout adapts to these changes, so fields can be added or removed anywhere. See Protobuf's [wire-safe changes](https://protobuf.dev/programming-guides/proto3/#wire-safe-changes) for the full list.

## Unsafe Changes

Unsafe migrations are also the same as in Protobuf: changing or reusing an existing field's number, or moving a field into an existing oneof. See Protobuf's [wire-unsafe changes](https://protobuf.dev/programming-guides/proto3/#wire-unsafe-changes) for the full list.

## Conditionally Unsafe Changes

Protobuf treats a few changes as conditionally safe, but because YaFF picks layouts dynamically it can't guarantee them, so by default it treats them as unsafe:

- **Changing a field's default value** (see [Default Values](overview.md#default-values)).
- **Changing a field's type to one of a different width**, such as `int32` to `int64`.
- **Turning a singular field into a `repeated` one.**

Some of these can be made safe by pinning the message to a specific layout, but Protobuf discourages all of them and many large codebases forbid them outright. See Protobuf's [do's and don'ts](https://protobuf.dev/best-practices/dos-donts/).

## Conditionally Safe Changes

These are wire-compatible across all YaFF layouts, but whether the data survives unchanged depends on the values themselves:

- **Switching between `string` and `bytes`.** They share one representation; reading `bytes` back as a `string` is fine
- **Switching an integer between its signed and unsigned form of the same width** (such as `int32` or `sint32` to `uint32`). The bits are preserved, but a negative value reads back as a large positive one, and vice versa.
- **Switching between a `map<K, V>` and a repeated message of key/value pairs.** The two share a representation, so you can move between them as long as the pair message mirrors the map entry (`key` = 1, `value` = 2).

### Changing Message Layout

A message uses the dynamic layout by default, but you can pin it to a static one (see [Controlling Layouts Statically](overview.md#controlling-layouts-statically)).

A dynamic reader can read data written in a static layout, but a static reader can only read its own, so the safe rollout order depends on the direction:

- **Specializing** (dynamic to static): update the writer first, then the readers. The new writer emits the static layout, which existing dynamic readers already understand, so by the time the readers expect it, that is what they receive.
- **Generalizing** (static back to dynamic): update the readers first, then the writer. Dynamic readers accept both the old static layout and whatever the new writer produces, so the writer can switch once every reader is dynamic.

The change is made with the same `layout` option:

```proto
message Reading {
    option (yaff.proto.message) = {layout: LAYOUT_FLAT};

    uint64 ts = 1;
    double value = 2;
}
```

`LAYOUT_FIXED` is the exception, because by design it forbids any schema modifications. So you can't change the layout of a message that is already fixed, and you can't add `LAYOUT_FIXED` to a message that is already in use. Both are breaking changes.
