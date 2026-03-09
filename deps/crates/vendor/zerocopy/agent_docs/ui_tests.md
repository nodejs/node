<!-- Copyright 2025 The Fuchsia Authors

Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
<LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
This file may not be copied, modified, or distributed except according to
those terms. -->

# UI & Output Tests

When updating UI test files (`tests/ui-*` or `zerocopy-derive/tests/ui-*`) or
functionality which could affect compiler error output or derive output, run:
`./tools/update-expected-test-output.sh`.

**Note:** We maintain separate UI tests for different toolchains (`ui-msrv`,
`ui-stable`, `ui-nightly`) because compiler output varies. The script handles
this automatically.

### Symlink Pattern

To share test code across toolchains while allowing for different error output,
we use a symlink pattern:

1.  **Canonical Source:** The `ui-nightly` directory holds the actual source
    files (`.rs`).
2.  **Symlinks:** The `ui-stable` and `ui-msrv` directories contain *symlinks*
    to the `.rs` files in `ui-nightly`.
    - **Example:** `tests/ui-stable/foo.rs` -> `../ui-nightly/foo.rs`
3.  **Unique Output:** Each directory contains its own `.stderr` files.

### Workflow Rules

- **Adding a Test:**
    1.  Create the `.rs` file in `ui-nightly`.
    2.  Create relative symlinks in `ui-stable` and `ui-msrv` pointing to the
        new file in `ui-nightly`.
    3.  Run `./tools/update-expected-test-output.sh` to generate the `.stderr` files.
- **Modifying a Test:**
    1.  Edit the `.rs` file in `ui-nightly`.
    2.  Run `./tools/update-expected-test-output.sh` to update the `.stderr` files.
- **Removing a Test:**
    1.  Delete the `.rs` file from `ui-nightly`.
    2.  Delete the symlinks from `ui-stable` and `ui-msrv`.
    3.  Delete the corresponding `.stderr` files from all three directories.

**NEVER** edit `.stderr` files directly. Only update them via the script or the
commands it runs. If a `.stderr` file is causing a test failure and updating it
via tooling does not fix the failure, that indicates a bug.
