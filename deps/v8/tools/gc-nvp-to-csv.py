#!/usr/bin/env python3
#
# Copyright 2015 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

#
# This is a utility for generating csv files based on GC traces produced by
# V8 when run with flags --trace-gc --trace-gc-nvp.
#
# Usage: gc-nvp-to-csv.py [-h] [filename]
#


# for py2/py3 compatibility
from __future__ import print_function

import argparse
import sys
import gc_nvp_common


def process_trace(f):
  trace, keys = gc_nvp_common.parse_gc_trace_with_keys(f)
  if trace:
    print(*keys, sep=', ')
    for entry in trace:
      print(*(entry.get(key, '') for key in keys), sep=', ')


def main(args):
  if args.filename:
    with open(args.filename, 'rt') as f:
      process_trace(f)
  else:
    process_trace(sys.stdin)


if __name__ == '__main__':
  # Command line options.
  parser = argparse.ArgumentParser(
      description='Helper script for converting --trace-gc-nvp logs to CSV')
  parser.add_argument('filename', type=str, nargs='?', help='GC trace log file')
  args = parser.parse_args()
  # Call the main function.
  main(args)
