# Schema Limits

This page documents the structural, physical, and architectural limits of YaFF serialization. Where possible they align with [Protobuf limits](https://protobuf.dev/programming-guides/proto-limits/) to preserve ecosystem interoperability.

## Message Size

The maximum size of a single serialized YaFF message is **2 GiB**. This is rarely a real constraint: an individual domain object is expected to stay well below that size.

If you need to transfer or store collections large enough to exceed this limit, partition the dataset into blocks of up to 2 GiB each. Splitting it this way keeps memory bounded and lets consumers process blocks concurrently.

## Number of Fields

The maximum number of unique fields allowed within a single message schema is **8190**.

Protobuf allows up to 65535 fields per message, but YaFF's cap of 8190 is more than enough in practice. Portable schemas hit lower ceilings anyway: target language constraints such as JVM bytecode limits can fail compilation at just 2100 to 4100 fields (see [Protobuf limits](https://protobuf.dev/programming-guides/proto-limits/#fields)).

Furthermore, defining thousands of fields in a single message is a severe architectural [anti-pattern](https://protobuf.dev/best-practices/dos-donts/#lots-of-fields).

## Max Field Number

The maximum allowed field number in a YaFF message schema is **8190**, matching the maximum number of fields.

Unlike Protobuf, which allows field numbers up to [536870911](https://protobuf.dev/programming-guides/proto3/#assigning), YaFF caps them at 8190 to keep its layout metadata compact. This is one of the few constraints that may require manual intervention when migrating existing proto schemas. Large field numbers, which Protobuf often uses for metadata or custom extensions, must be [remapped](overview.md#reassigning-field-numbers) into the supported range.
