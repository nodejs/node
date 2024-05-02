#!/usr/bin/env python3
# Copyright 2023 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Checks for non-determinism in mksnapshot output by comparing output from
multiple runs. Expected to be called with arguments like:

<script> <report path> <gn gen dir> <gn out dir> <number of runs>

The script will look for <number of runs> versions of snapshot and embedded
builtins within the gn gen and out directories.
"""

import hashlib
import sys

from pathlib import Path

ERROR_TEXT = """
Non-deterministic %s.
To reproduce, run mksnapshot multiple times and compare or:
1) Set gn variable v8_verify_deterministic_mksnapshot = true
2) Build target verify_deterministic_mksnapshot.
"""

assert len(sys.argv) == 5
report = Path(sys.argv[1])
gendir = Path(sys.argv[2])
outdir = Path(sys.argv[3])
n_runs = int(sys.argv[4])


def md5(path):
  with open(path, 'rb') as f:
    return hashlib.md5(f.read()).digest()


def snapshot_file(i):
  return outdir / f'snapshot_blob_run_{i}.bin'


def builtins_file(i):
  return gendir / f'embedded_run_{i}.S'


def verify(file_fun, type):
  different_hashes = set(md5(file_fun(i)) for i in range(n_runs))
  if len(different_hashes) != 1:
    print(ERROR_TEXT % type)
    sys.exit(1)


verify(snapshot_file, 'shapshot')
verify(builtins_file, 'embedded builtins')

# Dummy output file needed when running an action target.
with open(report, 'w') as f:
  f.write('Deterministic mksnapshot.')
