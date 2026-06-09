# Untrusted Messages

YaFF performs no validation when it reads a message. `ReadMessage` overlays the bytes in place and trusts that they were produced by a YaFF serializer for the matching schema. There is no equivalent of FlatBuffers' [Verifier](https://flatbuffers.dev/languages/cpp/#access-of-untrusted-buffers): reading a malformed or hostile buffer is undefined behavior.

## Where is my Verify?

YaFF targets internal, high-throughput data processing, where messages come from trusted producers inside the same system. For that use case, a full verification pass is both:

- **Too costly:** it would walk the whole message and check every offset and length on the hot read path, defeating the zero-copy design.
- **Unnecessary:** untrusted buffers are not part of the threat model.

So reads stay free, on the assumption that the buffer is well formed.

## What Verification Actually Covers

A bounds-checking verifier is often assumed to validate a buffer in general. **It does not.** It checks one thing: that every offset and array length stays inside the buffer, so traversal cannot read out of bounds. It says nothing about the data itself, so a corrupt scalar, an out-of-range value, or a plausible but wrong field all pass verification unnoticed.

In other words, a verifier defends against structurally hostile buffers, not against corrupt data.

## Choosing the Right Guard

Match the tool to the actual risk:

- **Hostile input,** a buffer from an untrusted source, possibly crafted to exploit the reader: YaFF offers nothing against this for now, and a workload that has to defend against it is probably outside YaFF's intended use case.
- **Accidental corruption,** a bit flip in storage or transit: a checksum over the serialized bytes is cheaper than a full traversal and catches exactly the bit flips a structural verifier would miss. Verify the checksum before reading.

For now, both guards are left to you. YaFF keeps validation off the read path to preserve zero-copy performance; supporting opt-in verification or checksums inside the library is technically possible and may come later.
