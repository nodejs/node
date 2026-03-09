#!/usr/bin/env bash
#
# Copyright 2025 The Fuchsia Authors
#
# Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
# <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
# license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
# This file may not be copied, modified, or distributed except according to
# those terms.

set -eo pipefail

script_name="ci/check_actions.sh"

# Ensure action-validator is installed
if [ ! -x "$HOME/.cargo/bin/action-validator" ]; then
    echo "$script_name: action-validator not found, installing..." >&2
    # Install specific version to ensure reproducibility
    cargo install -q action-validator --version 0.8.0 --locked
fi
export PATH="$HOME/.cargo/bin:$PATH"

# Files to exclude from validation (e.g., because they are not Actions/Workflows)
# Use relative paths matching `find .github` output
EXCLUDE_FILES=(
    "./.github/dependabot.yml"
    "./.github/release.yml"
)

failed=0

# Use process substitution and while loop to handle filenames with spaces robustly
while IFS= read -r -d '' file; do
    # Check if file is in exclusion list
    for exclude in "${EXCLUDE_FILES[@]}"; do
        if [[ "$file" == "$exclude" ]]; then
            continue 2
        fi
    done

    if ! output=$(action-validator "$file" 2>&1); then
        echo "$script_name: ❌ Validation failed for $file" >&2
        echo "$output" | sed "s|^|$script_name:   |" >&2
        failed=1
    fi
done < <(find ./.github -type f \( -iname '*.yml' -o -iname '*.yaml' \) -print0)

if [[ $failed -ne 0 ]]; then
    echo "$script_name: One or more files failed validation." >&2
    exit 1
fi
