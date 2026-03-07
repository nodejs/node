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

read -r -d '' PYTHON_SCRIPT <<'EOF' || true
import sys
import json

def parse_version(v):
    """Converts a version string to a tuple of integers."""
    return tuple(map(int, v.split(".")))

def main():
    """
    Checks that the package's MSRV is strictly lower than any version
    specified in `package.metadata.build-rs`.
    """
    try:
        data = json.load(sys.stdin)
    except json.JSONDecodeError as e:
        print(f"Error parsing JSON from cargo metadata: {e}", file=sys.stderr)
        sys.exit(1)

    # Find the zerocopy package
    try:
        pkg = next(p for p in data["packages"] if p["name"] == "zerocopy")
    except StopIteration:
        print("Error: zerocopy package not found in metadata", file=sys.stderr)
        sys.exit(1)

    msrv_str = pkg.get("rust_version")
    if not msrv_str:
        print("Error: rust-version not found in Cargo.toml", file=sys.stderr)
        sys.exit(1)

    try:
        msrv = parse_version(msrv_str)
    except ValueError:
        print(f"Error: Invalid MSRV format: {msrv_str}", file=sys.stderr)
        sys.exit(1)

    build_rs_versions = (pkg.get("metadata") or {}).get("build-rs", {})

    failed = False
    for name, ver_str in build_rs_versions.items():
        try:
            ver = parse_version(ver_str)
        except ValueError:
            print(f"Warning: Skipping invalid version format for {name}: {ver_str}", file=sys.stderr)
            continue

        # Check that MSRV < Version (strictly lower)
        if not (msrv < ver):
            print(f"Error: MSRV ({msrv_str}) is not strictly lower than {name} ({ver_str})", file=sys.stderr)
            failed = True

    if failed:
        sys.exit(1)

if __name__ == "__main__":
    main()
EOF

cargo metadata --format-version 1 --no-deps | python3 -c "$PYTHON_SCRIPT"
