#!/usr/bin/env python
# Copyright 2020 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Compare two folders and print any differences between files to both a
results file and stderr.

Specifically we use this to compare the output of Torque generator for
both x86 and x64 (-m32) toolchains.
"""

import difflib
import filecmp
import itertools
import os
import sys

assert len(sys.argv) > 3

folder1 = sys.argv[1]
folder2 = sys.argv[2]
results_file_name = sys.argv[3]

with open(results_file_name, "w") as results_file:
  def write(line):
    # Print line to both results file and stderr
    sys.stderr.write(line)
    results_file.write(line)

  def has_one_sided_diff(dcmp, side, side_list):
    # Check that we do not have files only on one side of the comparison
    if side_list:
      write("Some files exist only in %s\n" % side)
      for fl in side_list:
        write(fl)
    return side_list

  def has_content_diff(dcmp):
    # Check that we do not have content differences in the common files
    _, diffs, _ = filecmp.cmpfiles(
          dcmp.left, dcmp.right,
          dcmp.common_files, shallow=False)
    if diffs:
      write("Found content differences between %s and %s\n" %
        (dcmp.left, dcmp.right))
      for name in diffs:
        write("File diff %s\n" % name)
        left_file = os.path.join(dcmp.left, name)
        right_file = os.path.join(dcmp.right, name)
        with open(left_file) as f1, open(right_file) as f2:
          diff = difflib.unified_diff(
              f1.readlines(), f2.readlines(),
              dcmp.left, dcmp.right)
          for l in itertools.islice(diff, 100):
            write(l)
        write("\n\n")
    return diffs

  dcmp = filecmp.dircmp(folder1, folder2)
  has_diffs = has_one_sided_diff(dcmp, dcmp.left, dcmp.left_only) \
    or has_one_sided_diff(dcmp, dcmp.right, dcmp.right_only) \
    or has_content_diff(dcmp)

if has_diffs:
  sys.exit(1)
