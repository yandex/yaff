# Schema Overview

Protobuf is the standard for describing structured data, so projects often already have a data model written in `.proto`. YaFF generates code directly from those schemas, so you keep a single source of truth and reuse your existing definitions instead of maintaining a second schema. New projects can start from a fresh `.proto` just as well, since a proto schema is a portable description of your data model that stays useful for integrating with other systems.

This page covers how to describe your data in a YaFF schema, from plain Protobuf to YaFF's own options. Layouts and schema evolution have their own pages: [Layouts](layouts.md) and [Schema Evolution](evolution.md).

If you're new to Protobuf schemas, the [Protobuf documentation](https://protobuf.dev/programming-guides/) is a good place to start.

## Basic Usage

The schema below is a regular `.proto` file that exercises the Protobuf features YaFF supports, plus one YaFF option (`layout`). The subsections below walk through each feature.

```proto
syntax = "proto3";

package example;

import "yaff/proto/options.proto";

enum Status {
    STATUS_UNKNOWN = 0;
    STATUS_ACTIVE = 1;
    STATUS_BANNED = 2;
}

message GeoPoint {
    option (yaff.proto.message) = {layout: LAYOUT_FIXED};

    float lat = 1;
    float lon = 2;
}

message Profile {
    message Preferences {
        optional string locale = 1;
        optional bool subscribed = 2;

        // 'theme' (field 3) was removed.
        reserved 3; reserved "theme";
    }

    uint64 id = 1;
    optional string name = 2;
    optional Status status = 3;
    optional bool verified = 4;
    optional double rating = 5;
    optional bytes avatar = 6;

    repeated string tags = 7;
    repeated GeoPoint locations = 8;
    map<string, string> attributes = 9;

    optional Preferences preferences = 10;

    oneof contact {
        string email = 11;
        string phone = 12;
    }

    optional int32 score = 13 [deprecated = true];
}
```

### Field Numbers

By default, YaFF inherits field numbers from your `.proto`: a field's Protobuf number becomes its YaFF identifier. As in Protobuf, the numbers must be unique within a message and must never be reused once assigned.

A field number's size overhead depends on the layout: some layouts add none at all, while others store smaller numbers more compactly than larger ones. Whatever the layout, give the smallest numbers to the fields you populate most often. See [Layouts](layouts.md) for the specifics.

Gaps between field numbers can add overhead in some layouts. They work fine as is, but you can reclaim the space by reassigning a compact id (see [Reassigning Field Numbers](#reassigning-field-numbers)).

### Reserved Numbers

A field can be retired in three ways, and your choice affects which layouts the message can use (see [Layouts](layouts.md)):

- **`[deprecated = true]`** keeps the field in the schema but hides it from the generated YaFF interfaces. This leaves YaFF with more information about the field, so all layouts remain available.
- **`reserved`** removes the field, losing its type information, so not all layouts remain available.
- **Deleting the field** behaves the same way as `reserved`. Protobuf advises against it, since the field number could later be reused (see the [Protobuf guide](https://protobuf.dev/programming-guides/proto3/#deleting)).

If you must completely remove a field via `reserved` but still want to keep all layout options open, you can explicitly provide the missing type information to YaFF. For details, see [Preserving Types for Reserved](#preserving-types-for-reserved) in the Advanced section.

### Field Presence

Protobuf fields have two kinds of presence:

- **Implicit**: the field doesn't track whether it was set, so a value equal to the default can't be told apart from an unset one. `repeated` and `map` fields always work this way.
- **Explicit**: the field tracks whether it was set, so a value equal to the default is still distinguishable from unset. Message fields and oneof members always work this way.

For the remaining fields the default depends on the version: a proto3 singular scalar is implicit unless marked optional, a proto2 singular scalar is explicit, and editions follow the field_presence feature. See the [Protobuf presence guide](https://protobuf.dev/programming-guides/field_presence/) for details.

YaFF supports all of this, including the per-version presence defaults. Explicit-presence fields also get the matching generated accessors, such as hazzers (has_*). The cost depends on the layout: implicit presence is always free, while explicit presence is free in some layouts and costs a little in others.

### Scalar Types

YaFF supports all of Protobuf's scalar types (`int32`, `int64`, `uint32`, `uint64`, `sint32`, `sint64`, `fixed32`, `fixed64`, `sfixed32`, `sfixed64`, `float`, `double`, `bool`, `string`, `bytes`), and each maps one-to-one to its Protobuf type.

How a scalar is stored depends on the message layout. For now, no layout uses Protobuf's varint encoding: in the flat layouts, numeric scalars sit inline as raw, fixed-width values, so reading one is a direct memory access rather than a decode step. The variable-length `string` and `bytes` are stored out of line instead of inlined. Layouts that encode scalars more compactly may be added later.

#### Default Values

In proto2, a scalar field can set a custom default; proto3 fields always use the type's zero value. YaFF honors these defaults.

Changing a default later is only [conditionally safe](evolution.md): whether old data still reads correctly depends on the layout. YaFF chooses layouts dynamically, so it can't tell when the change is safe and **treats any default change as breaking**. Protobuf discourages it too (see the [Protobuf guide](https://protobuf.dev/best-practices/dos-donts/#change-default-value)).

### Enum Types

YaFF supports enums and all of their Protobuf features, including aliases (`allow_alias`). A value is stored as an `int32`.

Protobuf has two kinds of enum:

- **Closed**: a numeric value with no matching name is dropped, and the field reads back as unset.
- **Open**: a numeric value with no matching name is kept as is.

In Protobuf, proto2 enums are closed and proto3 enums are open, though the exact behavior can vary by implementation. See the [Protobuf enum guide](https://protobuf.dev/programming-guides/enum/) for details. **YaFF makes every enum open by design, so enum values are never lost.** This avoids the surprises that come with closed enums and differences between Protobuf versions.

Without an explicit default, an enum field defaults to 0.

### Message Types

Message fields, nested message definitions, and imports all work as they do in Protobuf. The one thing worth knowing is that layout is chosen per message: a nested message can use a different layout from the one that contains it, like the `LAYOUT_FIXED` `GeoPoint` inside `Profile` above.

### Oneof

YaFF supports `oneof` with the same semantics as Protobuf: the members are mutually exclusive, and at most one is set at a time.

### Maps

YaFF supports `map` fields. To keep the buffer flat and zero-copy, a map is stored as an array of key/value pairs sorted by key rather than as a hash table. Lookups use binary search, and iteration runs in key order, whereas Protobuf leaves map order unspecified. Maps are therefore best kept small, which they usually are.

### Well-Known Types

The well-known types (`Any`, `Timestamp`, `Struct`, the wrappers, and so on) are plain messages, so YaFF handles them like any other: you can read and write them and convert losslessly between Protobuf and YaFF.

It doesn't interpret them, though. An `Any` stays a `type_url` and an opaque blob rather than a message you can read directly, and a `Timestamp` is just its `seconds` and `nanos`. A proper zero-copy representation is planned, and the format already leaves room for it without breaking existing buffers.

## Advanced Usage

A plain `.proto` works with YaFF out of the box, with no extra options. For maximum efficiency, though, the options in this section give you finer control over how your data is represented.

### Reassigning Field Numbers

A field's YaFF id defaults to its Protobuf number, so a large gap in your Protobuf numbers becomes a large gap in YaFF ids, which can cost space depending on the layout. To avoid that, override the id with `(yaff.proto.field) = {id: N}`: keep the Protobuf number as the contract, give YaFF a compact id, and reserve that id so nothing else takes it.

```proto
message Event {
    uint64 id = 1;
    string name = 2;

    // Keep the Protobuf number, but assign a compact YaFF id...
    uint64 updated_at = 1000 [(yaff.proto.field) = {id: 3}];

    // ...and reserve number 3 so it can't be assigned to another field.
    reserved 3;
}
```

### Preserving Types for Reserved

As mentioned in the [Reserved Numbers](#reserved-numbers) section, using standard Protobuf `reserved` statements drops the field's type information, which restricts your choice of layouts. To remove a field from the public API while keeping full layout flexibility, you can explicitly declare the deleted field's type and label using the `reserved_fields` option inside `(yaff.proto.message)`:

```proto
message Document {
    option (yaff.proto.message) = {
        reserved_fields: [
            {id: 3, type: TYPE_STRING, label: LABEL_REPEATED}
        ]
    };

    uint64 id = 1;
    optional string title = 2;
}
```

The `type` and `label` values are Protobuf's standard `TYPE_*` and `LABEL_*` enums from [`descriptor.proto`](https://protobuf.dev/reference/protobuf/google.protobuf/#field-kind).


### Controlling Layouts Statically

By default, YaFF uses dynamic layout, which lets the runtime automatically pick the best representation based on the populated data. If you need absolute predictability or want to save a few CPU cycles on runtime layout dispatching, you can lock a message to a specific layout using the `layout` option inside `(yaff.proto.message)`:

```proto
message GeoPoint {
    option (yaff.proto.message) = {layout: LAYOUT_FIXED};

    float lat = 1;
    float lon = 2;
}
```

For the exact performance profiles and layout trade-offs, see [Layouts](layouts.md).

If your access patterns change, transitioning a message from a static layout back to dynamic is **conditionally safe**. See [Schema Evolution](evolution.md) for the specific compatibility rules.

### Schema Slices

When different consumers only need specific subsets of your data model, you can use schema slices to generate multiple independent runtime representations from a single `.proto` file. This lets you ship lightweight buffers to consumers that don't need the full payload.

To define a slice, you must annotate every message and field along the path from the root down to your data using the `(yaff.proto.message)` and `(yaff.proto.field)` options:

```proto
message Embedding {
    option (yaff.proto.message) = {slice_name: "banner_light"};
    option (yaff.proto.message) = {slice_name: "banner_heavy"};

    // Shared across both slices using its original Protobuf field number
    optional uint64 vector_id = 1 [
        (yaff.proto.field) = {slice_name: "banner_light"},
        (yaff.proto.field) = {slice_name: "banner_heavy"}
    ];

    optional uint64 model_version = 2;

    // Included only in the heavy slice, with an overridden YaFF identifier
    optional uint64 computed_time = 3 [
        (yaff.proto.field) = {slice_name: "banner_heavy", id: 2}
    ];
}
```

By default, omitting a field's id preserves its original Protobuf number within the slice, while omitting `slice_name` automatically includes the field in all slices defined for that message.

Each slice is generated separately; you select the slice and its namespace at generation time (the `TAG` and `NAMESPACE` parameters of `yaff_generate`).

#### Controlling Schema Slices

Advanced configuration options like `layout` or `reserved_fields` accept an optional `slice_name` parameter. This allows you to fine-tune layout strategies for each slice:

```proto
message BannerProfile {
    // This configuration only applies to the "banner_heavy" slice
    option (yaff.proto.message) = { slice_name: "banner_heavy", layout: LAYOUT_FLAT};
}
```

Options are resolved slice-first: for a given slice, an option tagged with its `slice_name` wins over an untagged fallback, and two options that share a `slice_name` (including the empty one) on the same field or message are a generation error. When one schema drives both default generation and slices, the untagged option doubles as the default-generation option and the per-slice fallback, so set `slice_name` explicitly everywhere to avoid ambiguity.

## Feature Support

The table below summarizes which Protobuf features YaFF supports today, which are partial, and which are planned. Features follow the Protobuf [Language Guide](https://protobuf.dev/programming-guides/proto3/) taxonomy.

| Feature | Status | Notes |
| :--- | :--- | :--- |
| Syntax proto2/proto3/editions | Yes | |
| Scalar Types | Yes | |
| Nested Types | Yes | |
| Repeated Fields | Yes | |
| Oneofs | Yes | |
| Maps | Yes | Stored as a key-sorted array of pairs, not a hash table |
| Enums | Yes | Always open; `allow_alias` supported; stored as `int32` |
| Default Values | Yes | Changing a default is [breaking](#default-values) |
| Reserved Fields | Yes | |
| Implicit and Explicit Presence | Yes | |
| Unknown Field Retention | Partial | Preserved when a buffer is passed through zero-copy; lost when materialized into a proto with a partial schema |
| Well-known Types (`Any`, `Timestamp`, etc) | Partial | Handled as opaque messages today; zero-copy interpretation is planned |
| Groups (proto2) | No | Legacy proto2 feature |
| Extensions (proto2) | No | Commonly used for metadata; YaFF serializes only declared fields, so extension values are dropped |
| Services and RPC Definitions | No | Out of scope for a wire format |

## Options Reference

The complete list of message and field configuration parameters is available directly in [options.proto](https://github.com/yandex/yaff/blob/main/include/yaff/proto/options.proto).
