#!/usr/bin/env bash
#
# Copyright 2026 The Fuchsia Authors
#
# Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
# <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
# license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
# This file may not be copied, modified, or distributed except according to
# those terms.

set -eo pipefail

# Directories to search
DIRS=("tests" "zerocopy-derive/tests")

EXIT_CODE=0

for dir in "${DIRS[@]}"; do
    if [ ! -d "$dir" ]; then
        echo "Warning: Directory $dir does not exist, skipping." >&2
        continue
    fi

    # Find all .stderr files
    while IFS= read -r -d '' stderr_file; do
        # Strip .stderr extension
        base="${stderr_file%.stderr}"

        # Strip toolchain suffixes
        base="${base%.msrv}"
        base="${base%.stable}"
        base="${base%.nightly}"

        # Construct the corresponding .rs file path
        rs_file="${stderr_file%.stderr}.rs"
        rs_file="${base}.rs"

        # Check if the .rs file exists. The `-e` flag checks if file exists:
        # It returns true for regular files and valid symlinks, and false for
        # broken symlinks or missing files.
        if [ ! -e "$rs_file" ]; then
            echo "Error: Orphaned stderr file found: $stderr_file" >&2
            echo "       Missing regular file or valid symlink: $rs_file" >&2
            EXIT_CODE=1
        fi
    done < <(find "$dir" -name "*.stderr" -print0)
done

if [ "$EXIT_CODE" -eq 1 ]; then
    echo "Found stale .stderr files. Please delete them." >&2
fi

exit "$EXIT_CODE"
