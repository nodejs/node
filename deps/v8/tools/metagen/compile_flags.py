# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Recover the libclang parse flags for the metagen harvest.

One entry point per build system:

  * GN (`get_compile_args_from_gn_desc`): query one representative
    target with `gn desc <out> <target> --format=json` and reconstruct
    its flags from the reported defines / include_dirs / cflags /
    cflags_cc -- exactly the fields the cxx tool template expands
    ({{defines}} {{include_dirs}} {{cflags}} {{cflags_cc}}).

  * Bazel (`get_compile_args_from_file`): read the single-entry
    compile_commands.json the rule synthesizes from the cc toolchain
    (bazel/defs.bzl). The entry always carries an `arguments` array, so
    the flags are read directly with no shell tokenization.

Both hand the cflags to libclang VERBATIM -- no cl-vs-gcc spelling
translation, which would need a patch per toolchain. The filter below
drops only categories that are meaningless or harmful for
`-fsyntax-only` (output paths, dep-info, backend options, plugin chains,
instrumentation, warnings-as-errors) or that libclang rejects (input
file, response files).

The clang builtin headers (stddef.h etc.) are not handled here: the
build system passes their directory explicitly via metagen.py's
--clang-builtin-headers-dir. Probing for them would read paths the
build never declared, which a sandboxed action cannot do.
"""

from __future__ import annotations

import json
import os
import shutil
import subprocess
import sys

# Single-token drops: compile-only and dep-info flags (these only occur
# in the Bazel-synthesized command line; gn desc cflags never carry
# them), plus warnings-as-errors. Warnings are meaningless for a
# syntax-only harvest, and under /WX they would escalate to errors:
# libclang unconditionally injects -fno-spell-checking and
# -fallow-editor-placeholders (CIndex.cpp), which the cl-mode driver
# does not recognize and warns about. Dropping /WX (rather than
# suppressing individual warning groups) keeps all of that inert.
_DROP_SINGLE = frozenset({
    "-c",
    "-MMD",
    "-MD",
    "-MP",
    "/WX",
})

# Two-token drops: the flag plus its following argument.
_DROP_TWO = frozenset({
    "-Xclang",  # plugin chains: -Xclang -add-plugin -Xclang blink-gc-plugin
    "-mllvm",  # backend-only options; libclang can't honor them
    "-MF",
    "-MT",
    "-MQ",
    "-MJ",  # dep-info output paths
    "-o",  # output object path
})

# Single-token prefix drops: instrumentation flag families that
# libclang can't act on in -fsyntax-only, plus per-warning escalations
# (see _DROP_SINGLE).
#
# -fsanitize= stays. It reaches the preprocessor through
# __has_feature(address_sanitizer) et al., and libc++ cross-checks that
# against the -D_LIBCPP_INSTRUMENTED_WITH_ASAN the same build passes:
# keep the define and drop the flag and its <__debug_utils/sanitizers.h>
# #errors out. Only the ignore lists go, since they steer instrumentation
# and nothing else, and a syntax-only parse has no business failing over
# a file it would not read (see also -fno-sanitize-ignorelist in
# metagen.py, which suppresses the implicit ones).
#
# The -fmodule*/-fimplicit-module* family goes too. A build with
# use_clang_modules compiles against prebuilt .pcm files and passes
# -fno-implicit-modules to forbid building them on the fly; those .pcms
# are real build artifacts the harvest neither has nor can produce, so
# the parse dies on the first modular header ("module 'X' is needed but
# has not been provided"). Dropping the whole family (not just
# -fno-implicit-modules) keeps the harvest textual: leaving -fmodules on
# would let libclang build implicit modules mid-harvest instead.
_DROP_PREFIX = (
    "-Werror",  # bare and -Werror=<warning>
    "-fcrash-diagnostics-dir=",
    "-fprofile-",
    "-fcoverage-",
    "-fsanitize-ignorelist=",
    "-fsanitize-system-ignorelist=",
    "-fsanitize-blacklist=",  # the pre-LLVM-13 spelling
    "-fmodule",  # -fmodules, -fmodule-file=, -fmodule-map-file=, ...
    "-fno-implicit-module",  # -fno-implicit-modules, ...-module-maps
)

_CXX_INPUT_EXTS = (".cc", ".cpp", ".cxx", ".cppm", ".c++", ".C")


def _find_gn(source_root: str) -> str | None:
  """Locate the gn binary. Both trees ship it under buildtools/<plat>/,
  except on the hosts whose DEPS entry excludes the CIPD package
  (s390x, ppc64, z/OS); there a system gn on PATH is the only one, which
  is what tools/mb/mb.py falls back to on the same hosts."""
  exe = "gn.exe" if sys.platform == "win32" else "gn"
  if sys.platform == "darwin":
    # Both Mac arches: the arch is in the CIPD package name, not the dir.
    plats = ("mac",)
  elif sys.platform == "win32":
    plats = ("win",)
  else:
    plats = ("linux64",)
  for plat in plats:
    p = os.path.join(source_root, "buildtools", plat, exe)
    if os.path.isfile(p) and os.access(p, os.X_OK):
      return p
  return shutil.which("gn")


def _gn_source_root(build_dir: str) -> str | None:
  """Return GN's source root: the nearest ancestor of build_dir holding
  a `.gn` marker (GN's canonical root definition). This differs between
  layouts -- the V8 checkout standalone, the Chromium `src/` root under
  Chromium -- and is what `//` in gn desc output resolves against."""
  d = os.path.abspath(build_dir)
  while True:
    if os.path.isfile(os.path.join(d, ".gn")):
      return d
    parent = os.path.dirname(d)
    if parent == d:
      return None
    d = parent


def _is_clang_cl(arg0: str) -> bool:
  """Detect clang-cl driver by argv[0]'s basename."""
  name = os.path.basename(arg0).lower()
  if name.endswith(".exe"):
    name = name[:-4]
  return name == "clang-cl"


def _filter(args: list[str], input_path: str) -> list[str]:
  out: list[str] = []
  norm_input = os.path.normpath(input_path) if input_path else ""
  i = 0
  while i < len(args):
    a = args[i]
    if a in _DROP_TWO:
      i += 2
      continue
    if a in _DROP_SINGLE:
      i += 1
      continue
    if any(a.startswith(p) for p in _DROP_PREFIX):
      i += 1
      continue
    if a.startswith("@"):
      # An @-file means the compile command references an unexpanded
      # response file; bail loudly rather than silently miss half the
      # flags. gn desc reports expanded cflags, and the Bazel rule emits
      # a fully-expanded `arguments` array, so this should never fire.
      raise RuntimeError(
          f"[metagen] unexpanded response file in compile flags: {a}.")
    if (norm_input and a.endswith(_CXX_INPUT_EXTS) and
        os.path.normpath(a) == norm_input):
      i += 1
      continue
    out.append(a)
    i += 1
  return out


def get_compile_args_from_gn_desc(
    build_dir: str, target_label: str) -> tuple[list[str], str, bool]:
  """GN path: reconstruct one target's compile flags via `gn desc`.

  Returns (flags, cwd, cl_mode):
    flags    libclang args. Path-bearing flags (-I, -isystem, ...) are
             left as-is; the caller must invoke libclang with cwd=`cwd`
             so build-dir-relative paths resolve.
    cwd      The build dir (cflags' paths are relative to it).
    cl_mode  True iff the toolchain is clang-cl. The caller injects
             `--driver-mode=cl` when this is set.

  cflags/cflags_cc are passed through verbatim; include_dirs are
  source-absolute `//...` and rebased to absolute here.
  """
  source_root = _gn_source_root(build_dir)
  if source_root is None:
    raise RuntimeError(
        f"[metagen] no .gn source-root marker found above {build_dir}.")
  gn = _find_gn(source_root)
  if not gn:
    raise RuntimeError(
        "[metagen] gn not found under <source-root>/buildtools or on PATH.")
  out_rel = os.path.relpath(os.path.abspath(build_dir), source_root)
  try:
    # -q ("don't print output on success") keeps stdout to the JSON alone.
    # Without it gn prepends any build-file warning to the document -- an
    # arm_float_abi that no declare_args() claims on the arm64 bots, say --
    # and the parse below fails. A gn that actually fails still reports.
    proc = subprocess.run(
        [gn, "desc", "-q", out_rel, target_label, "--format=json"],
        cwd=source_root,
        capture_output=True,
        text=True,
        check=True,
    )
  except FileNotFoundError:
    raise RuntimeError(f"[metagen] gn binary not found: {gn}")
  except subprocess.CalledProcessError as e:
    raise RuntimeError(f"[metagen] `gn desc {out_rel} {target_label}` failed "
                       f"(exit {e.returncode}). Has `gn gen` run there?\n"
                       f"{(e.stderr or '').strip()}")
  try:
    desc = json.loads(proc.stdout)
  except json.JSONDecodeError as e:
    raise RuntimeError(f"[metagen] `gn desc` output was not valid JSON: {e}")
  if not isinstance(desc, dict) or not desc:
    raise RuntimeError(
        f"[metagen] `gn desc` returned no target for {target_label}.")
  # Single-target query: the sole value maps the (toolchain-qualified)
  # label to its resolved fields.
  fields = next(iter(desc.values()))

  flags: list[str] = [f"-D{d}" for d in fields.get("defines") or []]
  for inc in fields.get("include_dirs") or []:
    if inc.startswith("//"):
      abs_inc = os.path.normpath(os.path.join(source_root, inc[2:]))
    elif os.path.isabs(inc):
      abs_inc = inc
    else:
      abs_inc = os.path.normpath(os.path.join(source_root, inc))
    flags.append(f"-I{abs_inc}")
  cflags = (fields.get("cflags") or []) + (fields.get("cflags_cc") or [])
  flags += cflags

  # clang-cl spells its options with a leading slash; posix clang never
  # does. The caller injects --driver-mode=cl when this is set.
  cl_mode = any(f.startswith("/") for f in cflags)

  # Drop plugin chains (-Xclang -add-plugin ...), backend-only (-mllvm),
  # and sanitizer/coverage/crash-dir flags libclang can't honor under
  # -fsyntax-only. gn desc's cflags never carry -c/-o/@rsp/the input
  # path, so the input-path arg to _filter is unused.
  filtered = _filter(flags, "")
  return filtered, os.path.abspath(build_dir), cl_mode


def get_compile_args_from_file(path: str) -> tuple[list[str], str, bool]:
  """Bazel path: read the single-entry compile_commands.json the rule
  synthesizes (bazel/defs.bzl).

  Returns (flags, cwd, cl_mode) with the same contract as
  `get_compile_args_from_gn_desc`. The synthesized entry always carries
  an `arguments` array (never a `command` string), so the flags are read
  directly with no shell tokenization. argv[0] and the bogus source file
  are dropped; `-c`/`-o` and friends go through the shared _filter.
  """
  with open(path) as f:
    entries = json.load(f)
  if not isinstance(entries, list) or len(entries) != 1:
    n = len(entries) if isinstance(entries, list) else "n/a"
    raise RuntimeError(f"[metagen] expected a one-entry list in {path}, got "
                       f"{type(entries).__name__} (len {n}).")
  entry = entries[0]
  args = entry.get("arguments")
  if not args:
    raise RuntimeError(
        f"[metagen] compile-commands entry has no `arguments` array: "
        f"{entry}. bazel/defs.bzl must emit `arguments`, not `command`.")
  arg0, rest = args[0], args[1:]
  cl_mode = _is_clang_cl(arg0)
  cwd = entry.get("directory") or "."
  return _filter(rest, entry.get("file", "")), cwd, cl_mode
