# Versioning

This page describes how YaFF is versioned and what stability to expect between releases.

## Numbering Scheme

YaFF's version is `major.minor.patch`, read from the `VERSION` file at the repository root. The scheme leaves room for full [semantic versioning](https://semver.org), but YaFF is still pre-`1.0.0`: with only a C++ support today, the interface may still change as development continues.

## Matching the Support Library and Generated Code

The generated code and the support library it is built against must be the **exact same version**. Every generated header bakes in the version it was produced with and compares it, at compile time, against the support library's `YAFF_VERSION` macro, so a mismatch is a compile error.

`YAFF_VERSION` is computed from that file as `major * 10000 + minor * 100 + patch` (so `0.1.0` is `100`). When you upgrade YaFF, regenerate your code and rebuild everything that uses it.

## Wire Format Stability

The wire format changes under a single rule: a new version may add new layouts, but never alters the encoding of an existing one. Two consequences follow:

- An existing layout's encoding is fixed, so a buffer that uses it stays readable by every later version (backward compatibility).
- A new layout is understood only by the version that introduced it and later ones, so an older support library cannot read a buffer that uses it (no forward compatibility for newly added layouts).

To keep rollouts safe, new layouts will be opt-in behind a feature flag, so producers do not start emitting one before every reader can handle it.
