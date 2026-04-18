#!/usr/bin/env python3

"""Invoke cargo with the correct Rust target on Windows cross-compilation.

Works around three issues: GYP's MSVS generator mangles executable names in
host-toolset actions (breaking direct cargo calls), GYP cannot set different
variable values per toolset, and cargo's output directory layout uses Rust
target triples while MSBuild expects platform names (x64/arm64). This script
reads MSBuild's $(Platform) to select the right Rust target and copies the
built library to a path MSBuild can resolve per-project.
"""

import os
import shutil
import subprocess
import sys


def main():
    # Arguments: <platform> <output_dir> [cargo_args...]
    if len(sys.argv) < 3:
        print('Usage: cargo_build.py <platform> <output_dir> [cargo_args...]', file=sys.stderr)
        sys.exit(1)

    platform = sys.argv[1]     # x64 or arm64
    output_dir = sys.argv[2]   # SHARED_INTERMEDIATE_DIR
    cargo_args = sys.argv[3:]
    build_profile = 'release' if '--release' in cargo_args else 'debug'

    cargo = os.environ.get('CARGO', 'cargo')
    if not os.path.isabs(cargo) and shutil.which(cargo) is None:
        home = os.environ.get('USERPROFILE', '')
        cargo_home = os.path.join(home, '.cargo', 'bin', 'cargo.exe')
        if os.path.isfile(cargo_home):
            cargo = cargo_home

    rust_target_map = {
        'x64': 'x86_64-pc-windows-msvc',
        'arm64': 'aarch64-pc-windows-msvc',
    }
    rust_target = rust_target_map.get(platform)
    if rust_target is None:
        print(f'Unsupported platform: {platform}', file=sys.stderr)
        sys.exit(1)

    cmd = [cargo, 'rustc', '--target', rust_target, '--target-dir', output_dir] + cargo_args
    ret = subprocess.call(cmd)
    if ret != 0:
        sys.exit(ret)

    # Copy output to the platform-specific directory that MSBuild expects.
    src = os.path.join(output_dir, rust_target, build_profile, 'node_crates.lib')
    dst_dir = os.path.join(output_dir, platform, build_profile)
    os.makedirs(dst_dir, exist_ok=True)
    dst = os.path.join(dst_dir, 'node_crates.lib')
    shutil.copy2(src, dst)


if __name__ == '__main__':
    main()
