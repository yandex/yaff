# Contributing

Thanks for your interest in contributing to YaFF! This document explains how to
report issues, propose changes, and submit pull requests.

## Reporting issues

We use GitHub Issues to track bug reports and feature requests: open a new one [here](https://github.com/yandex/yaff/issues/new). Please file issues in English so they are accessible to the whole community, and search existing issues first to avoid duplicates.

A good bug report includes:

- A clear description of the expected vs. actual behavior.
- A minimal reproducible example (ideally a small `.proto` schema and code snippet).
- Your environment: OS, compiler and version, CMake version, and YaFF version.
- Relevant build logs, stack traces, or error output.

For feature requests, describe the use case and the problem you are trying to solve, not just the proposed solution — this helps us find the best fit for the project.

## Contributing patches

We use Pull Requests to receive patches from external contributors. Each non-trivial pull request should be linked to an issue.

Before submitting a PR:

- Make sure all tests build and pass (enable them with `-DYAFF_BUILD_TESTS=ON`).
- Cover new behavior with tests; for a bug fix, add a test that reproduces it.
- Format your changes with `clang-format` (config in [`.clang-format`](.clang-format)).

## License

By contributing, you agree that your contributions are licensed under the [Apache License 2.0](LICENSE).

## Other questions

If you have any questions, feel free to discuss them in an issue.
Alternatively, you may send email to the Yandex Open Source team at opensource-support@yandex-team.ru.
