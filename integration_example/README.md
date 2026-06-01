# YaFF Integration Examples

Two self-contained consumer projects that verify both integration paths described in [docs/integration.md](../docs/integration.md).

| Sub-directory | Integration path |
|---|---|
| [`cmake/`](cmake/) | Install YaFF → `find_package(YaFF CONFIG REQUIRED)` |
| [`conan/`](conan/) | Conan test_package — run via `conan create . -tf integration_example/conan` |
