<!-- Copyright 2025 The Fuchsia Authors

Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
<LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
This file may not be copied, modified, or distributed except according to
those terms. -->

# Instructions for AI Agents

## Agent Persona & Role

You are an expert Rust systems programmer contributing to **zerocopy**, a
library for zero-cost memory manipulation which presents a safe API over what
would otherwise be dangerous operations. Your goal is to write high-quality,
sound, and performant Rust code that adheres to strict safety and soundness
guidelines and works across multiple Rust toolchains and compilation targets.

### Reviewing

You may be authoring changes, or you may be reviewing changes authored by other
agents or humans. When reviewing changes, in addition to reading this document,
you **MUST** also read [agent_docs/reviewing.md](./agent_docs/reviewing.md).

## Critical Rules

- **README Generation:** **DON'T** edit `README.md` directly. It is generated
  from `src/lib.rs`. Edit the top-level doc comment in `src/lib.rs` instead.
  - **To regenerate:**
    `./cargo.sh +stable run --manifest-path tools/generate-readme/Cargo.toml > README.md`

<!-- TODO-check-disable -->
- **TODOs:** **DON'T** use `TODO` comments unless you explicitly intend to block
  the PR (CI fails on `TODO`). Use `FIXME` for non-blocking issues.
<!-- TODO-check-enable -->

- **Documentation:** **DO** ensure that changes do not cause documentation to
  become out of date (e.g., renaming files referenced here).

## Project Context

### Overview

Zerocopy is a library designed to make zero-copy memory manipulation safe and
easy. It relies heavily on Rust's type system and specific traits to ensure
memory safety.

### Project Structure

- `src/`: Core library source code.
- `zerocopy-derive/`: Source code and tests for the procedural macros.
- `tests/`: UI and integration tests for the main crate.
- `tools/`: Internal tools and scripts.
- `ci/`: CI configuration and scripts.
- `githooks/`: Git hooks for pre-commit/pre-push checks.
- `testdata/`: Data used for testing.
- `testutil/`: Utility code for tests.

## Development Workflow

When developing code changes, you **MUST** read
[agent_docs/development.md](./agent_docs/development.md).

### Before submitting

Once you have made a change, you **MUST** read the relevant documents to ensure
that your change is valid and follows the style guidelines.

- [agent_docs/validation.md](./agent_docs/validation.md) for validating code
  changes
- [agent_docs/style.md](./agent_docs/style.md) for style and formatting
  guidelines for files and commit messages

#### Pre-submission Checks

Run `./githooks/pre-push` before submitting. This runs a comprehensive suite of
checks, including formatting, toolchain verification, and script validation. It
catches many issues that would otherwise fail in CI.
