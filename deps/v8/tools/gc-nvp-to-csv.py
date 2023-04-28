#!/usr/bin/env python3
#
# Copyright 2015 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

#
# This is an utility for generating csv files based on GC traces produced by
# V8 when run with flags --trace-gc --trace-gc-nvp.
#
# Usage: gc-nvp-to-csv.py <GC-trace-filename>
#


# for py2/py3 compatibility
from __future__ import print_function

import sys
import gc_nvp_common


def process_trace(filename):
  trace = gc_nvp_common.parse_gc_trace(filename)
  if len(trace):
    keys = trace[0].keys()
    print(', '.join(keys))
    for entry in trace:
      print(', '.join(map(lambda key: str(entry[key]), keys)))


if len(sys.argv) != 2:
  print("Usage: %s <GC-trace-filename>" % sys.argv[0])
  sys.exit(1)

process_trace(sys.argv[1])
