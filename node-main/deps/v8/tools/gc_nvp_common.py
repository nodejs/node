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
  for name, value in re.findall(r"([._\w]+)=([-\w]+(?:\.[0-9]+)?)", s):
    try:
      t[name] = float(value)
    except ValueError:
      t[name] = value
  return t


def split_nvp_with_keys(s):
  t, k = {}, []
  for name, value in re.findall(r"([._\w]+)=([-\w]+(?:\.[0-9]+)?)", s):
    try:
      t[name] = float(value)
    except ValueError:
      t[name] = value
    k.append(name)
  return t, k


def parse_gc_trace(input):
  # If input is string, treat it as a filename and open the file for reading.
  if isinstance(input, str):
    with open(input, 'rt') as f:
      return parse_gc_trace(f)
  # Otherwise, treat it as a file-like object.
  trace = []
  for line in input:
    info = split_nvp(line)
    if info and 'pause' in info and info['pause'] > 0:
      info['i'] = len(trace)
      trace.append(info)
  return trace


def parse_gc_trace_with_keys(input):
  # If input is string, treat it as a filename and open the file for reading.
  if isinstance(input, str):
    with open(input, 'rt') as f:
      return parse_gc_trace_with_keys(f)
  # Otherwise, treat it as a file-like object.
  trace = []
  keys = {'i'}
  key_list = ['i']
  for line in input:
    info, info_keys = split_nvp_with_keys(line)
    if info and 'pause' in info and info['pause'] > 0:
      info['i'] = len(trace)
      trace.append(info)
      for key in info_keys:
        if key not in keys:
          keys.add(key)
          key_list.append(key)
  return trace, key_list
