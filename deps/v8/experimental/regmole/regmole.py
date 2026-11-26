#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Driver script for the V8 regmole static register clobber analyzer."""

import argparse
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile

REPO_ROOT = Path(__file__).resolve().parents[2]
GCMOLE_CLANG_BIN = REPO_ROOT / "tools/gcmole/gcmole-tools/bin/clang++"
DEFAULT_CLANG_BIN = (
    GCMOLE_CLANG_BIN if GCMOLE_CLANG_BIN.exists() else Path("clang++"))
DEFAULT_PLUGIN_SO = REPO_ROOT / "experimental/regmole/libregmole.so"
DEFAULT_X64_ARGS_FILE = REPO_ROOT / "out/x64.release/v8_gcmole.args"
DEFAULT_ARM64_ARGS_FILE = REPO_ROOT / "out/arm64.release/v8_gcmole.args"

DIAG_RE = re.compile(r"^([^:\n]+):(\d+):(\d+):\s+(warning|error):\s+(.*)$")
EXPECT_RE = re.compile(r"//\s*EXPECT-WARNING:\s*(.+)$")


def infer_arch_for_file(source_file: str, default_arch: str) -> str:
  norm = source_file.replace("\\", "/")
  if "/arm64/" in norm or "arm64-test" in norm or norm.endswith("-arm64.cc"):
    return "arm64"
  if "/x64/" in norm or norm.endswith("-x64.cc"):
    return "x64"
  return default_arch


def load_compile_args(args_file: Path, arch: str) -> list[str]:
  if arch == "arm64" and DEFAULT_ARM64_ARGS_FILE.exists():
    raw = shlex.split(DEFAULT_ARM64_ARGS_FILE.read_text(encoding="utf-8"))
    return raw

  if not args_file.exists():
    raise FileNotFoundError(f"Compile args file not found: {args_file}")
  raw = shlex.split(args_file.read_text(encoding="utf-8"))
  if arch != "arm64":
    return raw

  arm64_args: list[str] = []
  for tok in raw:
    if tok == "-DV8_TARGET_ARCH_X64":
      arm64_args.append("-DV8_TARGET_ARCH_ARM64")
    else:
      arm64_args.append(tok.replace("out/x64.release", "out/arm64.release"))
  return arm64_args


def run_regmole_on_file(
    clang_bin: Path,
    plugin_so: Path,
    compile_args: list[str],
    source_file: str,
    plugin_args: list[str],
) -> subprocess.CompletedProcess[str]:
  cmd = [
      str(clang_bin),
      "-std=c++20",
      "-fsyntax-only",
      "-Wno-invalid-offsetof",
      "-DV8_ENABLE_REGMOLE",
      *compile_args,
      "-Xclang",
      "-load",
      "-Xclang",
      str(plugin_so),
      "-Xclang",
      "-plugin",
      "-Xclang",
      "regmole",
  ]
  for parg in plugin_args:
    cmd.extend(["-Xclang", "-plugin-arg-regmole", "-Xclang", parg])
  cmd.append(source_file)
  return subprocess.run(
      cmd,
      cwd=REPO_ROOT,
      stdout=subprocess.PIPE,
      stderr=subprocess.STDOUT,
      text=True,
      check=False,
  )


def verify_test_expectations(source_file: str, output: str) -> bool:
  src_path = REPO_ROOT / source_file
  lines = src_path.read_text(encoding="utf-8").splitlines()

  expected: dict[int, list[str]] = {}
  pending_patterns: list[str] = []
  for idx, line in enumerate(lines, start=1):
    m = EXPECT_RE.search(line)
    if m:
      pat = m.group(1).strip()
      stripped_before = line[:m.start()].strip()
      if stripped_before and not stripped_before.startswith("//"):
        expected.setdefault(idx, []).append(pat)
      else:
        pending_patterns.append(pat)
    elif pending_patterns and line.strip(
    ) and not line.strip().startswith("//"):
      expected.setdefault(idx, []).extend(pending_patterns)
      pending_patterns = []

  actual_warnings: dict[int, list[str]] = {}
  errors: list[str] = []
  for out_line in output.splitlines():
    m = DIAG_RE.match(out_line)
    if not m:
      continue
    diag_file, line_str, _col_str, sev, msg = m.groups()
    line_no = int(line_str)
    if sev == "error":
      errors.append(out_line)
    elif Path(diag_file).name == src_path.name:
      actual_warnings.setdefault(line_no, []).append(msg)
    else:
      errors.append(f"Unexpected warning outside test file: {out_line}")

  ok = True
  if errors:
    ok = False
    for err in errors:
      print(f"[FAIL] {source_file}: {err}")

  matched_actual: set[tuple[int, int]] = set()
  for exp_line, patterns in sorted(expected.items()):
    actual_on_line = actual_warnings.get(exp_line, [])
    for pat in patterns:
      found = False
      for j, act_msg in enumerate(actual_on_line):
        if (exp_line, j) not in matched_actual and pat in act_msg:
          matched_actual.add((exp_line, j))
          found = True
          break
      if not found:
        ok = False
        print(f"[FAIL] {source_file}:{exp_line}: missing expected warning "
              f"containing {pat!r} (got: {actual_on_line})")

  for act_line, msgs in sorted(actual_warnings.items()):
    for j, act_msg in enumerate(msgs):
      if (act_line, j) not in matched_actual:
        ok = False
        print(f"[FAIL] {source_file}:{act_line}: unexpected warning: {act_msg}")

  total_expected = sum(len(v) for v in expected.values())
  if ok:
    print(
        f"[PASS] {source_file}: all {total_expected} expected warnings matched "
        "with 0 unexpected warnings.")
  return ok


def main() -> int:
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument(
      "files",
      nargs="*",
      default=[
          "src/codegen/x64/macro-assembler-x64.cc",
          "src/baseline/baseline-compiler.cc",
      ],
      help="Source files to analyze with regmole.",
  )
  parser.add_argument(
      "--arch",
      choices=["x64", "arm64"],
      default="x64",
      help="Default target architecture when not inferred from file path.",
  )
  parser.add_argument(
      "--clang-bin",
      type=Path,
      default=DEFAULT_CLANG_BIN,
      help="Path to clang++ binary matching libregmole.so.",
  )
  parser.add_argument(
      "--plugin-so",
      type=Path,
      default=DEFAULT_PLUGIN_SO,
      help="Path to compiled libregmole.so.",
  )
  parser.add_argument(
      "--args-file",
      type=Path,
      default=DEFAULT_X64_ARGS_FILE,
      help="Path to x64 v8_gcmole.args file.",
  )
  parser.add_argument(
      "--test",
      action="store_true",
      help="Run both x64 and arm64 regmole expectation test suites.",
  )
  parser.add_argument(
      "--no-verify-annotations",
      action="store_true",
      help="Disable checking explicit annotations against inferred summaries.",
  )
  parser.add_argument(
      "--no-check-uses",
      action="store_true",
      help="Disable use-after-clobber and aliasing checks.",
  )
  parser.add_argument(
      "--no-check-maglev",
      action="store_true",
      help="Disable Maglev register allocator contract checks.",
  )
  parser.add_argument(
      "--cross-tu",
      action="store_true",
      help="Run 2-pass cross-TU summary export/import across input files.",
  )
  parser.add_argument(
      "--no-cross-tu",
      action="store_true",
      help="Disable automatic 2-pass cross-TU summary merging for 2+ files.",
  )
  parser.add_argument(
      "--export-summaries",
      type=Path,
      default=None,
      help="Export inferred function summaries and Maglev specs to a file.",
  )
  parser.add_argument(
      "--import-summaries",
      type=Path,
      default=None,
      help="Import inferred function summaries and Maglev specs from a file.",
  )
  args = parser.parse_args()

  base_plugin_args = []
  if args.no_verify_annotations:
    base_plugin_args.append("--no-verify-annotations")
  if args.no_check_uses:
    base_plugin_args.append("--no-check-uses")
  if args.no_check_maglev:
    base_plugin_args.append("--no-check-maglev")

  if args.test:
    test_suites = [
        ("experimental/regmole/regmole-test.cc", "x64"),
        ("experimental/regmole/regmole-arm64-test.cc", "arm64"),
    ]
    all_ok = True
    for test_file, test_arch in test_suites:
      c_args = load_compile_args(args.args_file, test_arch)
      p_args = [*base_plugin_args, f"--arch={test_arch}"]
      res = run_regmole_on_file(args.clang_bin, args.plugin_so, c_args,
                                test_file, p_args)
      if res.returncode != 0:
        sys.stdout.write(res.stdout)
        all_ok = False
        continue
      if not verify_test_expectations(test_file, res.stdout):
        all_ok = False
    return 0 if all_ok else 1

  files_with_arch = [
      (src, infer_arch_for_file(src, args.arch)) for src in args.files
  ]
  arch_counts: dict[str, int] = {}
  for _src, f_arch in files_with_arch:
    arch_counts[f_arch] = arch_counts.get(f_arch, 0) + 1

  exit_code = 0
  with tempfile.TemporaryDirectory(prefix="regmole_cross_tu_") as tmp_dir:
    tmp_path = Path(tmp_dir)
    merged_summaries_by_arch: dict[str, Path] = {}

    if not args.import_summaries and not args.no_cross_tu:
      for f_arch, count in arch_counts.items():
        if not args.cross_tu and count < 2:
          continue
        arch_files = [src for src, a in files_with_arch if a == f_arch]
        part_paths: list[Path] = []
        c_args = load_compile_args(args.args_file, f_arch)
        for idx, src in enumerate(arch_files):
          part = tmp_path / f"pass1_{f_arch}_{idx}.sum"
          p_args = [
              *base_plugin_args,
              f"--arch={f_arch}",
              "--no-check-uses",
              "--no-verify-annotations",
              f"--export-summaries={part}",
          ]
          print(f"=== Pass 1 (summaries) {src} ({f_arch}) ===")
          res = run_regmole_on_file(args.clang_bin, args.plugin_so, c_args, src,
                                    p_args)
          if res.returncode != 0:
            if res.stdout.strip():
              sys.stdout.write(res.stdout)
            exit_code = res.returncode
          elif part.exists():
            part_paths.append(part)
        if part_paths:
          merged = tmp_path / f"merged_{f_arch}.sum"
          with merged.open("w", encoding="utf-8") as out_f:
            for part in part_paths:
              out_f.write(part.read_text(encoding="utf-8"))
          merged_summaries_by_arch[f_arch] = merged

    for src, file_arch in files_with_arch:
      c_args = load_compile_args(args.args_file, file_arch)
      p_args = [*base_plugin_args, f"--arch={file_arch}"]
      import_path = args.import_summaries or merged_summaries_by_arch.get(
          file_arch)
      if import_path:
        p_args.append(f"--import-summaries={import_path}")
      if args.export_summaries:
        p_args.append(f"--export-summaries={args.export_summaries}")
      print(f"=== Analyzing {src} ({file_arch}) ===")
      res = run_regmole_on_file(args.clang_bin, args.plugin_so, c_args, src,
                                p_args)
      if res.stdout.strip():
        sys.stdout.write(res.stdout)
      if res.returncode != 0:
        exit_code = res.returncode
  return exit_code


if __name__ == "__main__":
  sys.exit(main())
