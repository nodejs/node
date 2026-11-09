#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Sets up shared gclient dependencies for V8 git worktrees.

When a worktree's DEPS matches the main repository, shared dependency
directories are symlinked directly to the main repository. When DEPS diverges,
a hermetic gclient sync is cached under <main_repo>/worktrees/.deps_cache/<hash>
and symlinked into the worktree.

Unreferenced cache entries are automatically pruned during sync while keeping
all actively referenced entries and at least MIN_CACHED_DEPS most recently used
entries. Cache management can also be triggered manually via the `prune-cache`
and `clear-cache` subcommands.
"""

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


MIN_CACHED_DEPS = 3


@contextmanager
def file_lock(lock_path: Path, blocking: bool = True):
  if not HAS_FCNTL:
    yield True
    return

  lock_path.parent.mkdir(parents=True, exist_ok=True)
  with open(lock_path, "a") as f:
    locked = False
    try:
      flags = fcntl.LOCK_EX | (0 if blocking else fcntl.LOCK_NB)
      fcntl.flock(f, flags)
      locked = True
    except (BlockingIOError, OSError):
      if blocking:
        raise
    try:
      yield locked
    finally:
      if locked:
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
          # shared_dirs can contain regular files (e.g. GEMINI.md), for which
          # copytree raises NotADirectoryError.
          if target.is_dir():
            shutil.copytree(
                target,
                dest,
                symlinks=True,
                copy_function=cow_copy,
                dirs_exist_ok=True)
          else:
            cow_copy(target, dest)
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
  if not worktree_dir.is_dir() or not (worktree_dir / ".git").exists():
    print(
        f"Error: {worktree_dir} is not an initialized git worktree.",
        file=sys.stderr)
    sys.exit(1)

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
      cache_dir = main_repo / "worktrees" / ".deps_cache"
      cache_root = cache_dir / content_hash
      cache_v8 = cache_root / "v8"

      with file_lock(cache_dir / ".lock"):
        if not cache_root.exists():
          init_deps_cache(main_repo, deps_worktree, shared_dirs, cache_root,
                          content_hash)

        cached_shared_dirs = get_shared_dirs(cache_root / ".gclient_entries")
        create_relative_symlinks(cache_v8, worktree_dir, cached_shared_dirs)
        record_worktree_usage(cache_root, worktree_dir)

  prune_cache(main_repo, blocking=False)


def record_worktree_usage(cache_root: Path, worktree_dir: Path):
  try:
    wt_file = cache_root / ".worktrees"
    wt_resolved = str(worktree_dir.resolve())
    existing = set()
    if wt_file.is_file():
      existing = {
          line.strip()
          for line in wt_file.read_text().splitlines()
          if line.strip()
      }
    if wt_resolved not in existing:
      existing.add(wt_resolved)
      wt_file.write_text("\n".join(sorted(existing)) + "\n")
    os.utime(cache_root, None)
  except OSError:
    pass


def get_referenced_cache_hashes(main_repo: Path, cache_dir: Path) -> set[str]:
  worktrees: set[Path] = set()
  try:
    out = subprocess.check_output(["git", "worktree", "list", "--porcelain"],
                                  cwd=main_repo,
                                  stderr=subprocess.DEVNULL,
                                  text=True)
    for line in out.splitlines():
      if line.startswith("worktree "):
        worktrees.add(Path(line[len("worktree "):].strip()))
  except Exception:
    pass

  worktrees_dir = main_repo / "worktrees"
  if worktrees_dir.is_dir():
    for d in worktrees_dir.iterdir():
      if d.name != ".deps_cache" and d.is_dir() and not d.is_symlink():
        worktrees.add(d)

  for item in cache_dir.iterdir():
    wt_file = item / ".worktrees"
    if item.is_dir() and wt_file.is_file():
      try:
        for line in wt_file.read_text().splitlines():
          if line.strip():
            worktrees.add(Path(line.strip()))
      except OSError:
        pass

  referenced: set[str] = set()
  main_resolved = main_repo.resolve()
  cache_resolved = cache_dir.resolve()
  deps_main = main_repo / "DEPS"

  for wt in worktrees:
    try:
      if not wt.is_dir() or not (wt / ".git").exists():
        continue
      if wt.resolve() == main_resolved:
        continue
    except OSError:
      continue

    build_link = wt / "build"
    if build_link.is_symlink():
      try:
        link_target = Path(
            os.path.normpath(build_link.parent.resolve() /
                             os.readlink(build_link)))
        rel = link_target.relative_to(cache_resolved)
        if rel.parts:
          referenced.add(rel.parts[0])
      except (OSError, ValueError):
        pass

    deps_wt = wt / "DEPS"
    if deps_wt.is_file():
      try:
        if not deps_main.is_file() or not filecmp.cmp(
            deps_main, deps_wt, shallow=False):
          h = hashlib.sha256(deps_wt.read_bytes()).hexdigest()[:12]
          referenced.add(h)
      except OSError:
        pass

  return referenced


def prune_cache(main_repo: Path,
                min_keep: int = MIN_CACHED_DEPS,
                blocking: bool = True):
  cache_dir = main_repo / "worktrees" / ".deps_cache"
  if not cache_dir.is_dir():
    return

  with file_lock(cache_dir / ".lock", blocking=blocking) as locked:
    if not locked:
      return

    entries = []
    for item in cache_dir.iterdir():
      if not item.is_dir():
        continue
      if item.name.endswith(".tmp"):
        shutil.rmtree(item, ignore_errors=True)
      else:
        try:
          entries.append((item.stat().st_mtime, item))
        except OSError:
          pass

    if len(entries) <= min_keep:
      return

    referenced = get_referenced_cache_hashes(main_repo, cache_dir)
    entries.sort(key=lambda x: x[0], reverse=True)

    keep = set(referenced)
    for _, item in entries[:min_keep]:
      keep.add(item.name)

    for _, item in entries:
      if item.name not in keep:
        shutil.rmtree(item, ignore_errors=True)


def clear_cache(main_repo: Path):
  cache_dir = main_repo / "worktrees" / ".deps_cache"
  if cache_dir.exists():
    print(f"Clearing worktree dependencies cache at {cache_dir}...")
    with file_lock(cache_dir / ".lock"):
      for item in cache_dir.iterdir():
        if item.is_dir():
          shutil.rmtree(item, ignore_errors=True)
    # We deliberately do not delete .lock because unlinking it while another
    # process might be waiting on it breaks mutual exclusion.
  else:
    print(f"No cache found at {cache_dir}.")


if __name__ == "__main__":
  if len(sys.argv) == 3 and sys.argv[1] == "clear-cache":
    clear_cache(Path(sys.argv[2]))
    sys.exit(0)
  if len(sys.argv) == 3 and sys.argv[1] == "prune-cache":
    prune_cache(Path(sys.argv[2]))
    sys.exit(0)
  if len(sys.argv) != 3 or sys.argv[1] in ("-h", "--help"):
    print("usage:\n"
          "  ./setup_worktree_build.py main_repo worktree_dir\n"
          "      Set up shared dependency symlinks for worktree_dir and prune\n"
          "      unreferenced .deps_cache entries.\n"
          "  ./setup_worktree_build.py prune-cache main_repo\n"
          "      Remove unreferenced .deps_cache entries, keeping all entries\n"
          f"      referenced by active worktrees and the {MIN_CACHED_DEPS} most"
          " recent entries.\n"
          "  ./setup_worktree_build.py clear-cache main_repo\n"
          "      Remove all entries in <main_repo>/worktrees/.deps_cache.")
    sys.exit(0 if len(sys.argv) > 1 and sys.argv[1] in ("-h", "--help") else 1)
  sync_dependencies(Path(sys.argv[1]), Path(sys.argv[2]))
