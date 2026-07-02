#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from contextlib import contextmanager
import filecmp
import hashlib
import os
import shutil
import subprocess
import sys
from pathlib import Path

try:
  import fcntl
  HAS_FCNTL = True
except ImportError:
  HAS_FCNTL = False


@contextmanager
def file_lock(lock_path: Path):
  if not HAS_FCNTL:
    yield
    return

  lock_path.parent.mkdir(parents=True, exist_ok=True)
  with open(lock_path, "w") as f:
    try:
      fcntl.flock(f, fcntl.LOCK_EX)
      yield
    finally:
      try:
        fcntl.flock(f, fcntl.LOCK_UN)
      except OSError:
        pass


def create_relative_symlinks(source_root: Path, target_dir: Path,
                             dirs: list[str]):
  for d in dirs:
    target = source_root / d
    link = target_dir / d
    if target.exists():
      link.parent.mkdir(parents=True, exist_ok=True)
      expected_rel = os.path.relpath(target.resolve(), link.parent.resolve())

      if link.is_symlink():
        try:
          if os.readlink(link) == expected_rel:
            continue
        except OSError:
          pass
        link.unlink()
      elif link.exists():
        print(
            f"Error: Target path {link} already exists as a regular file or directory. To enable shared dependencies symlinks, please remove this conflicting path manually.",
            file=sys.stderr)
        sys.exit(1)

      os.symlink(expected_rel, link, target_is_directory=target.is_dir())


def get_shared_dirs(root_entries_file: Path) -> list[str]:
  dirs = [
      "build",
      "buildtools",
      "base",
      "tools/clang",
      "tools/rust",
      "tools/luci-go",
      "test/wasm-js/tests",
      "test/wasm-spec-tests/tests",
      "GEMINI.md",
      ".agents",
  ]
  if root_entries_file.is_file():
    try:
      entries = {}
      exec(root_entries_file.read_text(), entries)
      dirs.extend(k[3:].split(':')[0]
                  for k in entries.get('entries', {})
                  if k.startswith("v8/"))
    except Exception as e:
      print(f"Warning: reading .gclient_entries failed: {e}", file=sys.stderr)
  unique_dirs = sorted(set(dirs))
  pruned = []
  for d in unique_dirs:
    if not any(d == p or d.startswith(p + "/") for p in pruned):
      pruned.append(d)
  return pruned


def init_deps_cache(main_repo: Path, deps_worktree: Path,
                    shared_dirs: list[str], cache_root: Path,
                    content_hash: str):
  # Step 1: Establish a staging area.
  tmp_root = cache_root.parent / f"{content_hash}.tmp"
  if tmp_root.exists():
    shutil.rmtree(tmp_root)
  tmp_root.mkdir(parents=True, exist_ok=True)
  tmp_v8 = tmp_root / "v8"

  try:
    # Step 2: Copy main repo via git clone --shared.
    subprocess.run(["git", "clone", "--shared",
                    str(main_repo),
                    str(tmp_v8)],
                   capture_output=True,
                   check=True)

    # Step 3: Overwrite the cloned main_repo DEPS with the diverged worktree DEPS.
    shutil.copy2(deps_worktree, tmp_v8 / "DEPS")

    # Step 4: Import deps from main repo using hardlinking for COW.
    def cow_copy(src, dst, *, follow_symlinks=True):
      src_str = Path(src).as_posix()
      is_git_pack = (".git/objects/pack" in src_str and src_str.endswith(
          (".pack", ".idx")))
      if is_git_pack:
        try:
          os.link(src, dst, follow_symlinks=follow_symlinks)
          return
        except OSError:
          pass
      shutil.copy2(src, dst, follow_symlinks=follow_symlinks)

    for d in shared_dirs:
      target = main_repo / d
      dest = tmp_v8 / d
      if target.exists() and not dest.exists():
        dest.parent.mkdir(parents=True, exist_ok=True)
        try:
          shutil.copytree(
              target,
              dest,
              symlinks=True,
              copy_function=cow_copy,
              dirs_exist_ok=True)
        except Exception as e:
          print(f"Warning during caching {d}: {e}", file=sys.stderr)

    # Step 5: Preserve custom gclient configurations.
    gclient_main = main_repo.parent / ".gclient"
    gclient_file = tmp_root / ".gclient"
    if gclient_main.is_file():
      shutil.copy2(gclient_main, gclient_file)
    else:
      print(
          "Error: .gclient file not found in the parent directory of the main repository.\n"
          "Please run `fetch v8` first in the main repository to set up your gclient workspace.",
          file=sys.stderr)
      sys.exit(1)

    # Step 6: Perform hermetic dependency sync inside the temporary staging directory.
    print("Initializing deps cache for diverged DEPS (~5-10 min)...")
    try:
      subprocess.run([
          "gclient", "sync", "--gclientfile=.gclient", "-D", "--force",
          "--reset"
      ],
                     cwd=tmp_root,
                     check=True)
    except subprocess.CalledProcessError as e:
      print(f"Error running gclient sync in cache: {e}", file=sys.stderr)
      sys.exit(1)

    # Step 7: Make the result available in the cache_root destination.
    try:
      tmp_root.replace(cache_root)
    except Exception:
      if not cache_root.exists():
        os.rename(tmp_root, cache_root)
  finally:
    # Clean up staging area if it was not moved/renamed to cache_root.
    if tmp_root.exists():
      shutil.rmtree(tmp_root, ignore_errors=True)


def should_skip_worktree_deps(worktree_dir: Path) -> bool:
  try:
    # Check if v8.skip-worktree-deps is set to true in git config
    res = subprocess.check_output(
        ["git", "config", "--get", "v8.skip-worktree-deps"],
        cwd=worktree_dir,
        stderr=subprocess.DEVNULL,
        text=True).strip()
    return res == "true"
  except Exception:
    return False


def sync_dependencies(main_repo: Path, worktree_dir: Path):
  if main_repo.resolve() == worktree_dir.resolve():
    return

  if should_skip_worktree_deps(worktree_dir):
    return

  # Protect the entire sync operation for this specific worktree
  # against concurrent runs of setup_worktree_build.py.
  worktree_lock = worktree_dir / ".setup_worktree.lock"
  with file_lock(worktree_lock):
    deps_main = main_repo / "DEPS"
    deps_worktree = worktree_dir / "DEPS"

    shared_dirs = get_shared_dirs(main_repo.parent / ".gclient_entries")

    if deps_main.is_file() and deps_worktree.is_file() and filecmp.cmp(
        deps_main, deps_worktree, shallow=False):
      create_relative_symlinks(main_repo, worktree_dir, shared_dirs)
    elif deps_worktree.is_file():
      content_hash = hashlib.sha256(deps_worktree.read_bytes()).hexdigest()[:12]
      cache_root = main_repo / "worktrees" / ".deps_cache" / content_hash
      cache_v8 = cache_root / "v8"

      lock_path = cache_root.parent / f"{content_hash}.lock"
      with file_lock(lock_path):
        if not cache_root.exists():
          init_deps_cache(main_repo, deps_worktree, shared_dirs, cache_root,
                          content_hash)

        cached_shared_dirs = get_shared_dirs(cache_root / ".gclient_entries")
        create_relative_symlinks(cache_v8, worktree_dir, cached_shared_dirs)


def clear_cache(main_repo: Path):
  cache_dir = main_repo / "worktrees" / ".deps_cache"
  if cache_dir.exists():
    print(f"Clearing worktree dependencies cache at {cache_dir}...")
    for item in cache_dir.iterdir():
      if item.is_dir():
        # Strip .tmp suffix to acquire the correct lock during init_deps_cache.
        hash_name = item.name[:-4] if item.name.endswith(".tmp") else item.name
        lock_path = cache_dir / f"{hash_name}.lock"
        with file_lock(lock_path):
          shutil.rmtree(item, ignore_errors=True)
    # We deliberately do not delete .lock files because unlinking them
    # while another process might be waiting on them breaks mutual exclusion.
  else:
    print(f"No cache found at {cache_dir}.")


if __name__ == "__main__":
  if len(sys.argv) == 3 and sys.argv[1] == "clear-cache":
    clear_cache(Path(sys.argv[2]))
    sys.exit(0)
  if len(sys.argv) != 3:
    print("usage:\n"
          "  ./setup_worktree_build.py main_repo worktree_dir\n"
          "  ./setup_worktree_build.py clear-cache main_repo")
    sys.exit(1)
  sync_dependencies(Path(sys.argv[1]), Path(sys.argv[2]))
