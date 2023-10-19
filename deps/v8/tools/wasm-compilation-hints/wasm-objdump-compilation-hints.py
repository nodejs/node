#!/usr/bin/env python3

# Copyright 2019 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be found
# in the LICENSE file.

from __future__ import print_function
import argparse
import io
import sys

from wasm import *

def parse_args():
  parser = argparse.ArgumentParser(\
      description="Read compilation hints from Wasm module.")
  parser.add_argument("in_wasm_file", \
      type=str, \
      help="wasm module")
  return parser.parse_args()

if __name__ == "__main__":
  args = parse_args()
  in_wasm_file = args.in_wasm_file if args.in_wasm_file else sys.stdin.fileno()
  with io.open(in_wasm_file, "rb") as fin:
    read_magic_number(fin);
    read_version(fin);
    while True:
      id, bs = read_varuintN(fin)
      if id is None:
        break
      payload_length, bs = read_varuintN(fin)
      if id == CUSTOM_SECTION_ID:
        section_name_length, section_name_length_bs = read_varuintN(fin)
        section_name_bs = fin.read(section_name_length)
        if section_name_bs == "compilationHints":
          num_hints, bs = read_varuintN(fin)
          print("Custom section compilationHints with ", num_hints, "hints:")
          for i in range(num_hints):
            hint, bs = read_uint8(fin)
            print(i, " ", hex(hint))
        else:
          remaining_length = payload_length \
              - len(section_name_length_bs) \
              - len(section_name_bs)
          fin.read()
      else:
        fin.read(payload_length)
