#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper script to create a snapshot from a pre-built V8.

This script expects V8 to be already built and takes the build directory
as input. It produces a snapshot archive and seeds from recent commits.
"""

import argparse
import io
import json
import logging
import os
import pathlib
import re
import shutil
import subprocess
import tarfile
import tempfile
import time
from dataclasses import dataclass, asdict
from datetime import datetime
from typing import Any, Optional, Sequence

@dataclass
class GitCommit:
    hash: str
    message: str
    timestamp: datetime
    diff: str

OBJ_DIR = pathlib.Path("obj")
SRC_DIR = pathlib.Path("src")
SEEDS_PATH = pathlib.Path("seeds.json")

# List of directory names (relative to v8_dir) to skip during source copying.
_DENY_LIST = [
    "buildtools/reclient",
    "third_party/llvm-build",
    "third_party/rust-toolchain",
]

def get_recent_v8_commits(v8_dir: pathlib.Path, days: int = 30) -> list[str]:
    """Retrieves a list of recent V8 commit hashes.

    Args:
        v8_dir: Path to the V8 source root.
        days: Number of days to look back for commits.

    Returns:
        A list of commit hashes (SHA-1).
    """
    print(f"Fetching V8 commits from the last {days} days...")
    cmd = ["git", "log", f"--since={days} days ago", "--format=%H%n%s%n%b%x00"]
    result = subprocess.run(
        cmd, cwd=str(v8_dir), capture_output=True, text=True, check=True
    )
    output = result.stdout
    if not output:
        return []

    commit_hashes = []
    for entry in output.split("\0"):
        if not entry.strip():
            continue
        lines = entry.strip().splitlines()
        if not lines:
            continue
        commit_hash = lines[0]
        commit_hashes.append(commit_hash)
    print(f"Found {len(commit_hashes)} commits")
    return commit_hashes

def get_shared_libraries(binary_path: pathlib.Path) -> list[pathlib.Path]:
    """Determines the shared library dependencies of a binary using ldd.

    Args:
        binary_path: Path to the binary file.

    Returns:
        A list of paths to the discovered shared libraries.
    """
    try:
        env = os.environ.copy()
        env["LD_TRACE_LOADED_OBJECTS"] = "1"
        result = subprocess.run(
            ["ldd", str(binary_path)], env=env, capture_output=True, text=True
        )
        libs = []
        for line in result.stdout.splitlines():
            match = re.search(r"=>\s+(\/\S+)", line)
            if match:
                libs.append(pathlib.Path(match.group(1)))
        return libs
    except Exception as e:
        logging.warning(
            f"Failed to get shared libraries for {binary_path}: {e}"
        )
        return []

def format_commit(v8_dir: pathlib.Path, commit_id: str) -> GitCommit:
    """Formats commit information including message and diff.

    Args:
        v8_dir: Path to the V8 source root.
        commit_id: The commit hash to format.

    Returns:
        A GitCommit object containing the commit ID, message, timestamp
        and diff.
    """
    message = subprocess.check_output(
        ["git", "log", "-1", "--format=%B", commit_id],
        cwd=str(v8_dir),
        text=True,
    )
    commit_time = subprocess.check_output(
        ["git", "log", "-1", "--format=%ct", commit_id],
        cwd=str(v8_dir),
        text=True,
    ).strip()
    patch = subprocess.check_output(
        ["git", "show", commit_id], cwd=str(v8_dir), text=True
    )
    return GitCommit(
        hash=commit_id,
        message=message,
        timestamp=datetime.fromtimestamp(int(commit_time)),
        diff=patch
    )

def copy_v8_sources(
    v8_dir: pathlib.Path,
    build_dir: pathlib.Path,
    source_dir: pathlib.Path,
    deny_list: list[str],
) -> None:
    """Copies all source files from v8_dir to source_dir, respecting a
    deny_list.

    It skips .git directories and the build_dir, but specifically includes
    the 'gen' subdirectory within build_dir.

    Args:
        v8_dir: Path to the V8 source root.
        build_dir: Path to the build directory.
        source_dir: Path to the destination directory.
        deny_list: List of directory names (relative to v8_dir) to skip.
    """
    print(f"Copying V8 sources from {v8_dir} to {source_dir}...")

    # Normalize paths for easier comparison
    abs_v8_dir = v8_dir.resolve()
    abs_deny_list = {(abs_v8_dir / d).resolve() for d in deny_list}
    abs_build_dir = build_dir.resolve()
    abs_gen_dir = (build_dir / "gen").resolve()

    def should_skip(path: pathlib.Path) -> bool:
        abs_path = path.resolve()
        if abs_path.name == ".git":
            return True
        if abs_path in abs_deny_list:
            return True
        if abs_path == abs_build_dir:
            return True
        # Skip everything in build_dir EXCEPT for the gen directory
        is_in_build = abs_build_dir in abs_path.parents
        is_gen = abs_gen_dir in [abs_path] + list(abs_path.parents)
        if is_in_build and not is_gen:
            return True
        return False

    count = 0
    archive_extensions = {".tar.gz", ".tgz"}
    for root, dirs, files in os.walk(v8_dir):
        root_path = pathlib.Path(root)

        # Filter directories in-place to prevent os.walk from entering them
        dirs[:] = [d for d in dirs if not should_skip(root_path / d)]

        for file in files:
            file_path = root_path / file
            # Skip archive files
            if any(file.endswith(ext) for ext in archive_extensions):
                continue

            rel_path = file_path.relative_to(v8_dir)
            dest_path = source_dir / rel_path
            dest_path.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(file_path, dest_path)
            count += 1

    # Also explicitly copy the gen directory if it exists and wasn't covered
    if abs_gen_dir.exists():
        print(f"Copying generated files from {abs_gen_dir}...")
        for root, _, files in os.walk(abs_gen_dir):
            root_path = pathlib.Path(root)
            for file in files:
                if any(file.endswith(ext) for ext in archive_extensions):
                    continue
                file_path = root_path / file
                try:
                    rel_path = file_path.relative_to(abs_v8_dir)
                    dest_path = source_dir / rel_path
                    dest_path.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(file_path, dest_path)
                    count += 1
                except ValueError:
                    # If for some reason gen is not under v8_dir, skip it
                    continue

    print(f"Successfully copied {count} files.")

def save_snapshot_archive(
    archive_path: pathlib.Path,
    artifact_dir: pathlib.Path,
    source_dir: pathlib.Path,
    seeds: list[Any],
) -> None:
    """Creates a compressed tar archive containing the snapshot artifacts,
    sources, and seeds.

    Args:
        archive_path: The path where the resulting .tgz file should be saved.
        artifact_dir: Directory containing the build artifacts (binary,
            libs, etc.).
        source_dir: Directory containing the relevant source files.
        seeds: A list of seed objects for the fuzzer.
    """
    class DateTimeEncoder(json.JSONEncoder):
        def default(self, obj):
            if isinstance(obj, datetime):
                return obj.isoformat()
            if hasattr(obj, "__dataclass_fields__"):
                return asdict(obj)
            return super().default(obj)

    print(f"Creating snapshot archive at {archive_path}...")
    with tarfile.open(archive_path, "w:gz") as tar:
        # Seeds
        seeds_json = json.dumps(seeds, indent=2, cls=DateTimeEncoder)
        tarinfo = tarfile.TarInfo(SEEDS_PATH.as_posix())
        tarinfo.size = len(seeds_json)
        tar.addfile(tarinfo, fileobj=io.BytesIO(seeds_json.encode()))

        # Add artifacts and sources
        for directory, prefix in [
            (artifact_dir, OBJ_DIR),
            (source_dir, SRC_DIR),
        ]:
            for root, _, files in os.walk(directory):
                for file in files:
                    full_path = pathlib.Path(root, file)
                    rel_path = full_path.relative_to(directory)
                    tar.add(full_path, arcname=(prefix / rel_path).as_posix())

def main():
    parser = argparse.ArgumentParser(
        description="Create a snapshot from a pre-built V8."
    )
    parser.add_argument(
        "--v8_dir",
        required=True,
        type=pathlib.Path,
        help="Path to the V8 checkout.",
    )
    parser.add_argument(
        "--build_dir",
        required=True,
        type=pathlib.Path,
        help="Path to the V8 build directory.",
    )
    parser.add_argument(
        "--output_file",
        required=True,
        type=pathlib.Path,
        help="Full path to the output .tgz file.",
    )
    args = parser.parse_args()

    with tempfile.TemporaryDirectory() as tmp_dir_str:
        tmp_dir = pathlib.Path(tmp_dir_str)
        artifact_dir = tmp_dir / "artifacts"
        source_dir = tmp_dir / "sources"

        artifact_dir.mkdir()
        source_dir.mkdir()

        # Copy artifacts
        shutil.copy(args.build_dir / "d8", artifact_dir / "d8")
        shutil.copy(
            args.build_dir / "compile_commands.json",
            artifact_dir / "compile_commands.json",
        )
        for f in ["snapshot_blob.bin", "icudtl.dat"]:
            if (args.build_dir / f).exists():
                shutil.copy(args.build_dir / f, artifact_dir / f)

        # Copy all sources except filtered ones
        copy_v8_sources(args.v8_dir, args.build_dir, source_dir, _DENY_LIST)

        # Ensure wasm-module-builder is at the root for the fuzzer
        wasm_builder = args.v8_dir / "test/mjsunit/wasm/wasm-module-builder.js"
        if wasm_builder.exists():
            shutil.copy(wasm_builder, artifact_dir / "wasm-module-builder.js")

        # Shared libraries and patching
        lib_path = artifact_dir / "lib"
        lib_path.mkdir(exist_ok=True)
        for lib in get_shared_libraries(artifact_dir / "d8"):
            shutil.copy(lib, lib_path / lib.name)

        # Seeds generation
        seeds = []
        for sha in get_recent_v8_commits(args.v8_dir):
            try:
                seeds.append(format_commit(args.v8_dir, sha))
            except Exception as e:
                print(f"Error processing commit {sha}: {e}")

        # Ensure the output directory exists
        args.output_file.parent.mkdir(parents=True, exist_ok=True)
        save_snapshot_archive(args.output_file, artifact_dir, source_dir, seeds)
        print(f"Snapshot created successfully at {args.output_file}")

if __name__ == "__main__":
    main()
