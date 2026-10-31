#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generate testcases and run them through foozzie locally.

Configuration-specific stack or backtrack limits are reported as differences.
correctness_fuzzer.py skips them when the reference has no result.

  tools/regexp/correctness_fuzzer/run_locally_against_foozzie.py \\
      --d8 out/x64.release/d8 --files 50

Foozzie requires v8_build_config.json next to d8. Correctness-fuzzer builds
also contain cross-architecture d8 binaries for --second-d8. --keep retains
reports and timeouts for correctness_fuzzer.py --testcase.
"""

import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile

DIR = os.path.dirname(os.path.realpath(__file__))
FOOZZIE_DIR = os.path.join(DIR, "..", "..", "clusterfuzz", "foozzie")
FOOZZIE = os.path.join(FOOZZIE_DIR, "v8_foozzie.py")

# Exclude default pairs that require a simulator binary.
with open(os.path.join(FOOZZIE_DIR, "v8_fuzz_experiments_regexp.json")) as f:
  DEFAULT_PAIRS = sorted(
      set("%s:%s" % (first, second)
          for _, first, second, second_d8 in json.load(f)
          if second_d8 == "d8"))


def main():
  ap = argparse.ArgumentParser(
      description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
  ap.add_argument("--d8", required=True, help="d8 for both sides")
  ap.add_argument(
      "--second-d8", help="d8 for the second side, e.g. a cross-arch build")
  ap.add_argument("--files", type=int, default=20, help="files to generate")
  ap.add_argument(
      "--pair",
      action="append",
      metavar="FIRST:SECOND",
      help="foozzie config pair to compare; repeatable "
      "(default: %s)" % " ".join(DEFAULT_PAIRS))
  ap.add_argument("--cases-per-file", type=int)
  ap.add_argument(
      "--keep",
      metavar="DIR",
      help="copy files foozzie reported on or timed out on into DIR "
      "(default: a temp dir)")
  args = ap.parse_args()
  # Foozzie resolves relative d8 paths against its own directory.
  args.d8 = os.path.abspath(args.d8)
  if args.second_d8:
    args.second_d8 = os.path.abspath(args.second_d8)

  pairs = [p.split(":", 1) for p in (args.pair or DEFAULT_PAIRS)]
  keep = args.keep

  with tempfile.TemporaryDirectory(prefix="regexp_fuzz_") as gen:
    cmd = [
        sys.executable,
        os.path.join(DIR, "run.py"), "--input_dir", gen, "--output_dir", gen,
        "--no_of_files",
        str(args.files)
    ]
    if args.cases_per_file:
      cmd += ["--cases-per-file", str(args.cases_per_file)]
    subprocess.run(cmd, check=True)

    failures = timeouts = 0
    for i in range(args.files):
      testcase = os.path.join(gen, "fuzz-%d.js" % i)
      for first, second in pairs:
        cmd = [
            sys.executable, FOOZZIE, "--first-config", first, "--second-config",
            second, "--first-d8", args.d8, "--second-d8", args.second_d8 or
            args.d8, testcase
        ]
        p = subprocess.run(cmd, capture_output=True, text=True)
        # Foozzie returns success on timeout. Retain and count the testcase.
        timed_out = "T-I-M-E-O-U-T" in p.stdout
        if p.returncode == 0 and not timed_out:
          continue
        if timed_out:
          timeouts += 1
        else:
          failures += 1
        if keep is None:
          keep = tempfile.mkdtemp(prefix="regexp_foozzie_")
        os.makedirs(keep, exist_ok=True)
        dest = os.path.join(keep, "fuzz-%d-%s-%s" % (i, first, second))
        shutil.copy(testcase, dest + ".js")
        with open(dest + ".txt", "w") as f:
          f.write(p.stdout + p.stderr)
        head = [l for l in p.stdout.splitlines() if l.startswith("# V8 ")]
        print("fuzz-%d %s vs %s: %s" %
              (i, first, second, "timeout" if timed_out else "rc=%d %s" %
               (p.returncode, " | ".join(head[:3]))))
        sys.stdout.flush()

  print("done: %d files x %d pairs, %d foozzie report(s), %d timeout(s)%s" %
        (args.files, len(pairs), failures, timeouts,
         (", kept under %s" % keep) if keep and (failures or timeouts) else ""))
  return 1 if failures else 0


if __name__ == "__main__":
  sys.exit(main())
