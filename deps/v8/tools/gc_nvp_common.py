# Copyright 2015 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

#
# Common code for parsing --trace-gc-nvp output.
#


from __future__ import with_statement
import re

def split_nvp(s):
  t = {}
  for (name, value) in re.findall(r"([._\w]+)=([-\w]+(?:\.[0-9]+)?)", s):
    try:
      t[name] = float(value)
    except ValueError:
      t[name] = value

  return t


def parse_gc_trace(input):
  trace = []
  with open(input) as f:
    for line in f:
      info = split_nvp(line)
      if info and 'pause' in info and info['pause'] > 0:
        info['i'] = len(trace)
        trace.append(info)
  return trace
