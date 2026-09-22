#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Entry point for metagen instance-type generation.

Drives a libclang harvest of V8_OBJECT / V8_IT_-annotated C++ class
declarations and emits `instance-types.h`. One invocation per build
dir; ninja calls us from `tools/metagen/BUILD.gn`.

Layout-mode generation (`extern class Foo extends ...` Torque emission
from the same C++ headers) is a follow-up; the prototype lives on the
stacked `metagen-layout` branch and is not part of this CL.
"""

from __future__ import annotations

import argparse
import os
import sys

if __name__ == "__main__":
  # Running this file as a script -- ninja runs `python3
  # tools/metagen/metagen.py`, Bazel's launcher execs it out of the
  # runfiles tree -- puts tools/metagen/ on sys.path, not its parent, so
  # the `metagen` package is not importable yet.
  _PARENT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
  if _PARENT not in sys.path:
    sys.path.insert(0, _PARENT)

# Don't import cpp_hier yet -- it pulls in clang.cindex at module-load
# time, which requires the bindings to be resolvable. We bootstrap that
# in main() once the flags say where they come from.
from metagen import compile_flags  # noqa: E402
from metagen import instance_types as it  # noqa: E402


def _write_depfile(depfile: str, output: str, deps: list[str]) -> None:
  """Write a ninja depfile recording `deps` as inputs of `output`.

  The parse's inputs are not declared as action inputs -- the build
  file declares only the driver -- so this is what ties the output to
  the headers it was derived from.

  Paths are written relative to the process's cwd, which ninja sets to
  the build dir -- the directory it also resolves depfile paths
  against. Spaces are escaped; ninja's depfile parser treats an
  unescaped one as a separator.
  """

  def rel(p: str) -> str:
    return os.path.relpath(p).replace(" ", "\\ ")

  os.makedirs(os.path.dirname(os.path.abspath(depfile)) or ".", exist_ok=True)
  body = " \\\n  ".join(rel(d) for d in deps)
  with open(depfile, "w") as f:
    f.write(f"{rel(output)}: \\\n  {body}\n" if deps else f"{rel(output)}:\n")


_THIS_DIR = os.path.dirname(os.path.abspath(__file__))
_DEFAULT_V8_ROOT = os.path.abspath(
    os.path.join(_THIS_DIR, os.pardir, os.pardir))


def main() -> int:
  p = argparse.ArgumentParser()
  p.add_argument("--v8-root", default=_DEFAULT_V8_ROOT)
  p.add_argument(
      "--libclang-dir",
      default=None,
      help="Absolute or build-dir-relative path to the llvm-libclang "
      "package root (the dir containing bindings/python/ and "
      "lib/libclang.so -- i.e. .../third_party/llvm-libclang/). The GN "
      "and standalone-Bazel actions resolve and pass this; DEPS "
      "guarantees the package is present, so metagen does not search "
      "for it. Mutually exclusive with --libclang-from-python-env.")
  p.add_argument(
      "--libclang-from-python-env",
      action="store_true",
      help="Take clang.cindex from the Python environment instead, for "
      "build systems whose dependency graph supplies a self-configuring "
      "bindings module (it loads its own native libclang, so there is no "
      "path to pass). Opt-in on purpose: metagen never falls back to an "
      "ambient module just because --libclang-dir was omitted.")
  p.add_argument(
      "--libclang-so",
      default=None,
      help="Absolute path to a native libclang shared library, for "
      "platforms with no prebuilt llvm-libclang package.")
  p.add_argument(
      "--libclang-bindings-dir",
      default=None,
      help="Absolute path to the directory holding the clang.cindex "
      "bindings (the dir that contains clang/cindex.py). Required with, and "
      "only valid with, --libclang-so; the caller version-matches the two.")
  p.add_argument(
      "--expect-libclang-major",
      default=None,
      type=int,
      help="Clang toolchain major version (GN's clang_version). Used with "
      "--libclang-so to warn if the loaded libclang is older than the "
      "toolchain whose builtin headers metagen parses against.")
  p.add_argument(
      "--clang-resource-dir",
      default=None,
      help="Path to the clang toolchain's resource dir (lib/clang/<N>, "
      "the directory whose include/ holds stddef.h, stdarg.h and the arch "
      "intrinsics). Passed to the parse as -resource-dir=, which lets the "
      "driver order the builtin headers itself. Preferred over "
      "--clang-builtin-headers-dir wherever the canonical layout exists. "
      "Mutually exclusive with it.")
  p.add_argument(
      "--clang-builtin-headers-dir",
      default=None,
      help="Directory holding the clang builtin headers, for build systems "
      "that stage them flat, with no lib/clang/<N> hierarchy for "
      "-resource-dir= to point at (Bazel). Appended as a system-include dir "
      "instead, which takes the path directly. Never probed for: the build "
      "system knows where it staged them, and in a sandbox any path we "
      "guessed would be an undeclared input. Mutually exclusive with "
      "--clang-resource-dir.")
  p.add_argument(
      "--driver",
      required=True,
      help="Path to the checked-in C++ driver whose active direct includes "
      "are harvested.")
  p.add_argument(
      "--out",
      required=True,
      help="Directory to write instance-types.h into. The only "
      "directory metagen writes to.")
  p.add_argument(
      "--depfile",
      default=None,
      help="Path to write a ninja depfile recording the files the "
      "harvest read a class declaration, base or marker definition out "
      "of. Only the driver is a declared build input, so without this "
      "an edit to one of them leaves the output stale.")
  # TODO(jgruber): Remove --check once metagen is the sole IT source and the
  # Torque reference path (v8_use_metagen_instance_types=false) is gone.
  p.add_argument(
      "--check",
      action="store_true",
      help="Diff against the reference instance-types.h under "
      "out/x64.optdebug/gen/torque-generated/ and exit non-zero on "
      "mismatch. Comment-insensitive: the position comments point at "
      "C++ headers here but at .tq sources in Torque's emission.")
  p.add_argument(
      "--build-dir",
      default=None,
      help="Path to a configured V8 build dir (out/<config>). The tool "
      "runs `gn desc` against it (see --flags-from-target) to harvest the "
      "cflags clang uses for that target. Mutually exclusive with "
      "--compile-commands.")
  p.add_argument(
      "--source-root",
      default=None,
      help="GN source root (the directory containing the build's .gn file). "
      "Required with --build-dir.")
  p.add_argument(
      "--flags-from-target",
      default=None,
      help="GN label of a representative compiled target whose flags "
      "`gn desc` reports (e.g. //v8:v8_base_without_compiler). Required "
      "with --build-dir.")
  p.add_argument(
      "--flags-toolchain",
      default=None,
      help="GN label of the toolchain to resolve --flags-from-target in "
      "(e.g. //build/toolchain/linux:clang_x64). Defaults to the build's "
      "default toolchain, which is only right when the action and the "
      "target it queries share it.")
  p.add_argument(
      "--flags-dependency",
      default=None,
      help="File materializing the GN target flags, recorded in --depfile "
      "so a flag change reruns metagen. Required with --build-dir when "
      "--depfile is used.")
  p.add_argument(
      "--extra-flag",
      action="append",
      default=[],
      metavar="FLAG",
      help="A parse flag the build system supplies outside the target's "
      "cflags, repeatable. Windows: the SDK/UCRT include flags, which the "
      "toolchain interpolates into its tool command template rather than "
      "into cflags, so `gn desc` never reports them.")
  p.add_argument(
      "-v", "--verbose", action="store_true", help="Enable verbose logging.")
  p.add_argument(
      "--compile-commands",
      default=None,
      help="Path to a compile_commands.json the build system already "
      "wrote (Bazel emits one synthesized via cc_common). Same shape "
      "as compile_commands.json. Mutually exclusive with --build-dir.")
  args = p.parse_args()

  def verbose_print(msg: str) -> None:
    if args.verbose:
      print(msg, file=sys.stderr)

  v8_root = os.path.abspath(args.v8_root)

  # Deep V8 class hierarchies (js-objects.h et al.) overflow the
  # libclang ctypes visitor's default recursion budget. Cheap and
  # global; bump before any traversal, and before the bootstrap so it
  # applies whichever mode supplies the bindings.
  sys.setrecursionlimit(max(sys.getrecursionlimit(), 20000))

  _modes = [
      bool(args.libclang_dir),
      bool(args.libclang_from_python_env),
      bool(args.libclang_so)
  ]
  if sum(_modes) != 1:
    print(
        "error: exactly one of --libclang-dir, "
        "--libclang-from-python-env or --libclang-so is required.",
        file=sys.stderr)
    return 1
  # A matched pair: one without the other is always a misconfiguration.
  if bool(args.libclang_so) != bool(args.libclang_bindings_dir):
    print(
        "error: --libclang-so and --libclang-bindings-dir must be given "
        "together.",
        file=sys.stderr)
    return 1

  # Bootstrap libclang *before* importing cpp_hier -- it references
  # clang.cindex at module-load time.
  from metagen import clang_bootstrap  # noqa: E402
  if args.libclang_from_python_env:
    clang_bootstrap.bootstrap_from_python_env()
  elif args.libclang_so:
    clang_bootstrap.bootstrap_native(
        native_so=os.path.abspath(args.libclang_so),
        bindings_dir=os.path.abspath(args.libclang_bindings_dir),
        expect_major=args.expect_libclang_major)
  else:
    # GN and Bazel rebase the path against the build root before passing
    # it, so a relative value is relative to the current working
    # directory (which ninja/bazel set to the build root).
    # `os.path.abspath` honors that.
    clang_bootstrap.bootstrap(libclang_dir=os.path.abspath(args.libclang_dir))
  from metagen import cpp_hier  # noqa: E402

  if bool(args.build_dir) == bool(args.compile_commands):
    print(
        "error: exactly one of --build-dir or --compile-commands is "
        "required.",
        file=sys.stderr)
    return 1
  if bool(args.build_dir) != bool(args.source_root):
    print(
        "error: --build-dir and --source-root must be given together.",
        file=sys.stderr)
    return 1

  driver_path = os.path.abspath(args.driver)

  if args.build_dir:
    if not args.flags_from_target:
      print(
          "error: --flags-from-target is required with --build-dir.",
          file=sys.stderr)
      return 1
    build_dir = os.path.abspath(args.build_dir)
    flags_target = args.flags_from_target
    if args.flags_toolchain:
      flags_target = f"{flags_target}({args.flags_toolchain})"
    raw_flags, parse_cwd, cl_mode = compile_flags.get_compile_args_from_gn_desc(
        build_dir, flags_target, os.path.abspath(args.source_root))
    flags_source = f"build_dir={build_dir} (gn desc {flags_target})"
  else:
    cc_json = os.path.abspath(args.compile_commands)
    raw_flags, parse_cwd, cl_mode = compile_flags.get_compile_args_from_file(
        cc_json)
    flags_source = f"compile_commands={cc_json}"

  flags_dependency = None
  if args.flags_dependency:
    if not args.build_dir:
      print("error: --flags-dependency requires --build-dir.", file=sys.stderr)
      return 1
    flags_dependency = os.path.abspath(args.flags_dependency)
    if not os.path.isfile(flags_dependency):
      print(
          f"error: --flags-dependency does not exist: {flags_dependency}",
          file=sys.stderr)
      return 1
  elif args.build_dir and args.depfile:
    print(
        "error: --flags-dependency is required with --build-dir and "
        "--depfile.",
        file=sys.stderr)
    return 1

  # Builtin headers (stddef.h etc.). llvm-libclang ships only the .so and
  # bindings, so its own resource dir has no headers in it. We take them
  # from the clang toolchain package instead, which rolls from the same
  # LLVM revision and so always matches the parsing library.
  if bool(args.clang_resource_dir) == bool(args.clang_builtin_headers_dir):
    print(
        "error: exactly one of --clang-resource-dir or "
        "--clang-builtin-headers-dir is required.",
        file=sys.stderr)
    return 1
  resource_dir = None
  builtin_headers_dir = None
  if args.clang_resource_dir:
    resource_dir = os.path.abspath(args.clang_resource_dir)
    flag = "--clang-resource-dir"
    probe = os.path.join(resource_dir, "include", "stddef.h")
  else:
    builtin_headers_dir = os.path.abspath(args.clang_builtin_headers_dir)
    flag = "--clang-builtin-headers-dir"
    probe = os.path.join(builtin_headers_dir, "stddef.h")
  if not os.path.isfile(probe):
    print(
        f"[metagen] {flag} does not hold clang's builtin headers:\n"
        f"  {probe} not found",
        file=sys.stderr)
    return 1
  # -fsyntax-only, -ferror-limit=, -resource-dir= and -D carry `CLOption`
  # visibility in clang's Options.td, so one spelling works under both
  # drivers. -isystem does NOT: the clang-cl driver drops it with an
  # ignorable -Wunknown-argument warning, and the parse then fails on the
  # first intrinsics header. Its cl-mode equivalent is -imsvc.
  sysinclude = "-imsvc" if cl_mode else "-isystem"

  prefix = [
      "-fsyntax-only",
      # The harvest aborts on the first error either way, so this buys
      # nothing but diagnostics: uncapped, a failure we cannot reproduce
      # locally reports every error in one bot log rather than the first
      # 19 and a "too many errors emitted". Note the `/clang:` forward
      # form is silently dropped by libclang in cl-mode (the default cap
      # still fires); the bare form works.
      "-ferror-limit=0",
      # A sanitizer build's -fsanitize= flags are kept, so that
      # __has_feature() agrees with the -D_LIBCPP_INSTRUMENTED_WITH_*
      # the same build passes. Their ignore lists are not: clang looks for
      # the implicit ones in the resource dir, where we'd either find
      # nothing or read files this action never declared.
      "-fno-sanitize-ignorelist",
      # Tell src/objects/instance-type.h to skip its file-scope
      # references to IT symbols metagen hasn't emitted yet. See the
      # guards there.
      "-DV8_METAGEN_GENERATION_PASS",
      # Make DECL_PRINTER / DECL_VERIFIER expand to declarations in
      # every config (release builds define neither), so the harvest
      # can detect Name##Print / Name##Verify and the emitted
      # diagnostic-dispatch list is config-independent.
      "-DOBJECT_PRINT",
      "-DVERIFY_HEAP",
  ]
  if cl_mode:
    prefix.insert(0, "--driver-mode=cl")

  # Tracing declares nothing the harvest reads, but all-objects.h reaches
  # src/tracing/trace-event.h, whose perfetto path pulls in generated protos
  # (perfetto is on in Chromium builds). Taking the non-perfetto path instead
  # of ordering the action after protoc keeps the harvest at the front of the
  # build; the harvested set is the same either way. These have to follow the
  # queried flags to override the -D they carry, and the SDK and JSON-export
  # defines go with it because v8config.h requires them to imply
  # V8_USE_PERFETTO.
  no_perfetto = [
      "-UV8_USE_PERFETTO",
      "-UV8_USE_PERFETTO_JSON_EXPORT",
      "-UV8_USE_PERFETTO_SDK",
  ]

  # --extra-flag goes ahead of the queried flags, matching where the
  # toolchain's command template puts them.
  flags = prefix + args.extra_flag + raw_flags + no_perfetto

  # The builtin headers have to sit after libc++'s include dir and before
  # the platform SDK's.
  #
  # -resource-dir= lets the driver place them, and it gets that right in
  # both modes, so prefer it. That relies on libc++ arriving as -I, since
  # the driver sorts -imsvc dirs after the resource dir. Only a flat
  # staging dir needs the include-dir spelling, which we append: right for
  # libc++, and right on Windows only while the SDK dirs arrive as
  # /winsysroot.
  if resource_dir:
    flags.append(f"-resource-dir={resource_dir}")
  else:
    flags.append(f"{sysinclude}{builtin_headers_dir}")

  verbose_print(f"Harvesting class hierarchy from "
                f"{os.path.relpath(driver_path, v8_root)} ({flags_source})...")
  out_dir = os.path.abspath(args.out)
  os.makedirs(out_dir, exist_ok=True)
  path = os.path.join(out_dir, "instance-types.h")

  cpp_res = cpp_hier.scan_cpp(v8_root, driver_path, flags, parse_cwd=parse_cwd)
  verbose_print(f"  {len(cpp_res.classes)} classes, "
                f"{len(cpp_res.provenance)} provenance files (from C++)")
  classes = cpp_res.classes

  # The files the harvest read a fact out of, not everything the parse
  # opened. The closure is ~17x larger and mostly the inline layer and
  # the sysroot, none of which can move a class head; depending on it
  # would put the harvest on the critical path of edits that cannot
  # change its output. Provenance is derived from the walk rather than
  # filtered by hand, so it stays correct when a declaration moves --
  # and once metagen emits object layout, field types become one more
  # provenance source rather than a reason to widen the whole set.
  #
  # Written ahead of the write-if-changed check below: ninja requires
  # the depfile to exist after the action runs, including on the run
  # that leaves the output untouched. --check writes no output, so a
  # depfile naming one would be meaningless.
  if args.depfile and not args.check:
    deps = cpp_res.provenance
    if flags_dependency:
      deps = deps + [flags_dependency]
    _write_depfile(args.depfile, path, deps)

  # Solve the IT tree and emit instance-types.h: the IT enum macros,
  # the bucket lists that drive auto-emission of IsX predicates /
  # InstanceTypeTraits / heap snapshot labels, and the Print/Verify
  # dispatch list.
  tree = it.assign_instance_types(classes)
  generated = ("#ifndef V8_GEN_TORQUE_GENERATED_INSTANCE_TYPES_H_\n"
               "#define V8_GEN_TORQUE_GENERATED_INSTANCE_TYPES_H_\n"
               "\n" + it.emit_enum_macros(tree) + it.emit_bucket_macros(tree) +
               it.emit_dispatch_macro(tree) +
               "#endif  // V8_GEN_TORQUE_GENERATED_INSTANCE_TYPES_H_\n")

  if args.check:
    import difflib
    import re

    def normalize(text: str) -> list[str]:
      # Comment-insensitive comparison: /* ... */ position comments
      # legitimately differ (C++ header links here, .tq links in
      # Torque's emission), as do // prose comments and the include
      # guard / V8_USE_METAGEN_INSTANCE_TYPES scaffolding around the macro
      # blocks. The diagnostic-dispatch list is also skipped: it has no
      # literal counterpart in Torque's emission (the torque path
      # aliases it to the debug-reader lists, see
      # src/objects/instance-types-gen.h). Only the remaining macro
      # definitions must match.
      scaffold = re.compile(r"^#(ifndef V8_GEN_TORQUE_GENERATED_"
                            r"|define V8_GEN_TORQUE_GENERATED_"
                            r"|endif|if !V8_USE_METAGEN_INSTANCE_TYPES)")
      out: list[str] = []
      in_dispatch = False
      for ln in text.splitlines():
        if ln.startswith("#define HEAP_OBJECT_DIAGNOSTIC_DISPATCH_LIST"):
          in_dispatch = True
          continue
        if in_dispatch:
          # The block is the define plus `V(...) \` continuation lines.
          if not ln.rstrip().endswith("\\"):
            in_dispatch = False
          continue
        if scaffold.match(ln) or ln.lstrip().startswith("//"):
          continue
        ln = re.sub(r" +", " ", re.sub(r"/\*.*?\*/", "", ln)).rstrip()
        if ln:
          out.append(ln + "\n")
      return out

    # The torque path splits its emission: the IT enum lands in
    # instance-types.h, the bucket lists in
    # instance-type-checker-lists.h. metagen emits both into one file,
    # in the same order.
    ref_dir = os.path.join(v8_root, "out/x64.optdebug/gen/torque-generated")
    reference: list[str] = []
    for ref_name in ("instance-types.h", "instance-type-checker-lists.h"):
      ref_path = os.path.join(ref_dir, ref_name)
      if not os.path.exists(ref_path):
        print(f"[check] reference not found: {ref_path}", file=sys.stderr)
        return 2
      with open(ref_path) as f:
        reference += normalize(f.read())
    generated_normalized = normalize(generated)
    if generated_normalized == reference:
      print("[ok] instance-types.h matches reference", file=sys.stderr)
      return 0
    print("[diff] instance-types.h differs from reference", file=sys.stderr)
    for line in difflib.unified_diff(
        reference,
        generated_normalized,
        fromfile="reference",
        tofile="generated"):
      sys.stdout.write(line)
    return 1

  # Write-if-changed: the GN action rule carries restat=1, so leaving
  # the mtime alone when the content is identical prunes the (near-
  # global, via instance-type.h) set of dependents whenever an edit
  # to one of the input headers doesn't change the IT assignment.
  if os.path.exists(path):
    with open(path) as f:
      if f.read() == generated:
        verbose_print(f"Unchanged {path}")
        return 0
  with open(path, "w") as f:
    f.write(generated)
  verbose_print(f"Wrote {path}")
  return 0


if __name__ == "__main__":
  sys.exit(main())
