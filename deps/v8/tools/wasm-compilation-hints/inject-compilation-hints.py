#!/usr/bin/env python3

# Copyright 2019 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be found
# in the LICENSE file.

import argparse
import io
import sys

from wasm import *

FUNCTION_SECTION_ID = 3

def parse_args():
  parser = argparse.ArgumentParser(\
      description="Inject compilation hints into a Wasm module.")
  parser.add_argument("-i", "--in-wasm-file", \
      type=str, \
      help="original wasm module")
  parser.add_argument("-o", "--out-wasm-file", \
      type=str, \
      help="wasm module with injected hints")
  parser.add_argument("-x", "--hints-file", \
      type=str, required=True, \
      help="binary hints file to be injected as a custom section " + \
          "'compilationHints'")
  return parser.parse_args()

if __name__ == "__main__":
  args = parse_args()
  in_wasm_file = args.in_wasm_file if args.in_wasm_file else sys.stdin.fileno()
  out_wasm_file = args.out_wasm_file if args.out_wasm_file else sys.stdout.fileno()
  hints_bs = open(args.hints_file, "rb").read()
  with io.open(in_wasm_file, "rb") as fin:
    with io.open(out_wasm_file, "wb") as fout:
      magic_number, bs = read_magic_number(fin);
      fout.write(bs)
      version, bs = read_version(fin);
      fout.write(bs)
      num_declared_functions = None
      while True:
        id, bs = read_varuintN(fin)
        fout.write(bs)
        if id is None:
          break
        payload_length, bs = read_varuintN(fin)
        fout.write(bs)

        # Peek into function section for upcoming validity check.
        if id == FUNCTION_SECTION_ID:
          num_declared_functions, bs = peek_varuintN(fin)

        bs = fin.read(payload_length)
        fout.write(bs)

        # Instert hint section after function section.
        if id == FUNCTION_SECTION_ID:
          assert len(hints_bs) == num_declared_functions, "unexpected number of hints"
          write_compilation_hints_section(fout, hints_bs)
