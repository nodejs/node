# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Make `clang.cindex` importable and point it at a native libclang.

Kept separate so the bootstrap can run before any module that uses
`clang.cindex` is imported -- `cpp_hier.py` references
`cindex.CursorKind.X` at module-load time (in module-level tuples), so
its import requires `clang.cindex` to be resolvable from sys.path.

Usage from the metagen entry point:

    from metagen import clang_bootstrap
    clang_bootstrap.bootstrap(libclang_dir=args.libclang_dir)
    from metagen import cpp_hier   # now safe

There are three modes, and the caller must pick exactly one explicitly --
see the mutually exclusive --libclang-dir / --libclang-from-python-env /
--libclang-so flags in metagen.py. None is reachable by omitting an argument.

`bootstrap(libclang_dir=...)` -- bundled mode, used by GN and by
standalone Bazel. Bindings and native library both come from the
`llvm-libclang` package extracted at `third_party/llvm-libclang` (a
first-class GCS dep), version-matched to each other and to the V8
compiler roll:

    bindings/python/clang/{cindex,__init__}.py
    lib/libclang.so | lib/libclang.dylib | bin/libclang.dll

Sourcing both from one package (rather than the bindings from the clang
toolchain and the .so from rust-toolchain) keeps them locked together.
We look ONLY inside the package dir and hard-fail if it is absent.

`bootstrap_from_python_env()` -- environment mode, for build systems
whose Python dependency graph already provides a self-configuring
`clang.cindex` (it loads its own native library at import time, so
passing paths in would fight the wrapper). The version guarantee then
lives in that dependency edge rather than here; see the function.

Neither mode searches the system or falls back silently. A libclang
whose API version does not match the headers' expectations yields a
subtly wrong AST rather than a clean failure, and this generator's
output ships in the V8 build.

The clang builtin headers (stddef.h etc.) are a separate input handled
by metagen.py's --clang-builtin-headers-dir.
"""

from __future__ import annotations

import os
import re
import sys


def _parse_clang_major(version: str) -> int | None:
  """Pull the major version out of a `clang_getClangVersion()` string.

  The string is free-form (e.g. "clang version 21.1.8 (Fedora 21.1.8-1)"),
  so match the first dotted-number run rather than assume a fixed layout.
  """
  m = re.search(r"(\d+)\.\d+", version)
  return int(m.group(1)) if m else None


def bootstrap_from_python_env() -> None:
  """Use a `clang.cindex` the Python environment already provides.

  For build systems whose dependency graph supplies a self-configuring
  bindings module (it calls `Config.set_library_file` itself at import
  time). We deliberately do not touch sys.path or the Config here: the
  module owns that, and overriding it would fight the wrapper.

  A pip-installed `clang` package satisfies the import just as well and
  would parse with a libclang nothing pins, so reject that case and log
  the module we did accept.
  """
  try:
    import clang.cindex as cindex  # noqa: E402
  except ImportError as e:
    print(
        f"[metagen] --libclang-from-python-env was passed but "
        f"`import clang.cindex` failed: {e}\nThe build rule must put a "
        f"libclang bindings module on the Python path (e.g. as a "
        f"py_binary dep).",
        file=sys.stderr)
    sys.exit(1)

  origin = os.path.dirname(os.path.abspath(cindex.__file__))
  # site-packages / dist-packages means an interpreter-level install
  # (pip, distro package), never a build-declared dependency: the trees a
  # build stages its Python deps into do not use those directory names.
  if any(
      part in ("site-packages", "dist-packages")
      for part in origin.split(os.sep)):
    print(
        f"[metagen] refusing a system-installed clang.cindex:\n"
        f"  {origin}\n"
        f"--libclang-from-python-env expects the bindings to come from "
        f"the build's dependency graph, whose libclang version is "
        f"pinned. This path is an interpreter-level install, which "
        f"nothing pins. Declare the bindings as a dependency of the "
        f"metagen tool, or use --libclang-dir for the bundled package.",
        file=sys.stderr)
    sys.exit(1)

  # cindex resolves the native library lazily, so touching the Library
  # here surfaces a misconfigured wrapper now rather than mid-parse.
  try:
    cindex.Config().lib
  except Exception as e:
    print(
        f"[metagen] the environment-provided clang.cindex could not load "
        f"its native libclang: {e}\nThe bindings module is expected to "
        f"configure this itself; metagen passes no path in this mode.",
        file=sys.stderr)
    sys.exit(1)

  # In this mode the harvest log is the only record of which libclang
  # parsed, so emit it unconditionally.
  print(
      f"[metagen] using environment-provided clang.cindex from {origin} "
      f"(library: {cindex.Config.library_file or 'wrapper-configured'})",
      file=sys.stderr)


def bootstrap_native(native_so: str,
                     bindings_dir: str,
                     expect_major: int | None = None) -> None:
  """Point cindex at an explicit system libclang and a bindings dir.

  For platforms with no prebuilt llvm-libclang GCS package, the native
  library is the distro's own libclang and the cindex bindings are
  staged separately, so the two come from different paths rather than
  one package.

  `expect_major` is the clang toolchain major (GN's clang_version), if the
  loaded libclang reports an older major, warn it may be out of step
  with the builtin headers metagen parses against.
  """
  bindings = os.path.abspath(bindings_dir)
  if not os.path.isfile(os.path.join(bindings, "clang", "cindex.py")):
    print(
        f"[metagen] clang.cindex bindings not found under\n"
        f"  {bindings}\n(expected {os.path.join(bindings, 'clang', 'cindex.py')}"
        f"). Point --libclang-bindings-dir at the dir holding the clang/ "
        f"package.",
        file=sys.stderr)
    sys.exit(1)
  sys.path.insert(0, bindings)

  import clang.cindex as cindex  # noqa: E402

  if not os.path.isfile(native_so):
    print(
        f"[metagen] libclang native library not found:\n  {native_so}",
        file=sys.stderr)
    sys.exit(1)
  cindex.Config.set_library_file(native_so)

  # Log the libclang version and warn if it is older than the clang
  # toolchain, so a mismatch is discoverable.
  get_version = getattr(cindex.Config(), "get_clang_version", None)
  version = get_version() if get_version else None
  major = _parse_clang_major(version) if version else None
  print(
      f"[metagen] native libclang: {version or 'version unknown'}",
      file=sys.stderr)
  if major is not None and expect_major is not None and major < expect_major:
    print(
        f"[metagen] warning: libclang major {major} is older than the clang "
        f"toolchain ({expect_major}); the parse may not match its builtin "
        f"headers.",
        file=sys.stderr)


def bootstrap(libclang_dir: str) -> None:
  """Add the llvm-libclang package's Python bindings to sys.path and point
  cindex at its native libclang.

  `libclang_dir` is the package root (the dir holding bindings/python/ and
  lib/libclang.so). The GN and Bazel actions always pass it, resolved to
  third_party/llvm-libclang, whose presence DEPS guarantees. A missing file
  here is therefore a real error, not a layout to search around.
  """
  base = os.path.abspath(libclang_dir)

  # Python bindings: <package>/bindings/python/clang/cindex.py.
  bindings_dir = os.path.join(base, "bindings", "python")
  if not os.path.isfile(os.path.join(bindings_dir, "clang", "cindex.py")):
    print(
        f"[metagen] llvm-libclang Python bindings not found under\n"
        f"  {bindings_dir}\nRe-run `gclient sync` to fetch the llvm-libclang "
        f"package.",
        file=sys.stderr)
    sys.exit(1)
  sys.path.insert(0, bindings_dir)

  import clang.cindex as cindex  # noqa: E402

  # Native libclang: the package ships the loadable library next to the
  # bindings (lib/libclang.so | lib/libclang.dylib | bin/libclang.dll).
  if sys.platform == "darwin":
    native_rel = os.path.join("lib", "libclang.dylib")
  elif sys.platform == "win32":
    native_rel = os.path.join("bin", "libclang.dll")
  else:
    native_rel = os.path.join("lib", "libclang.so")
  native = os.path.join(base, native_rel)
  if not os.path.isfile(native):
    print(
        f"[metagen] llvm-libclang native library not found:\n"
        f"  {native}\nRe-run `gclient sync` to fetch the llvm-libclang "
        f"package.",
        file=sys.stderr)
    sys.exit(1)
  cindex.Config.set_library_file(native)
