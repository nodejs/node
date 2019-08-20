#!/usr/bin/env python

# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''
Converts a given file in clang assembly syntax to a corresponding
representation in inline assembly. Specifically, this is used to convert
embedded.S to embedded.cc for Windows clang builds.
'''

import argparse
import sys

def asm_to_inl_asm(in_filename, out_filename):
  with open(in_filename, 'r') as infile, open(out_filename, 'wb') as outfile:
    outfile.write('__asm__(\n')
    for line in infile:
      # Escape " in .S file before outputing it to inline asm file.
      line = line.replace('"', '\\"')
      outfile.write('  "%s\\n"\n' % line.rstrip())
    outfile.write(');\n')
  return 0

if __name__ == '__main__':
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('input', help='Name of the input assembly file')
  parser.add_argument('output', help='Name of the target CC file')
  args = parser.parse_args()
  sys.exit(asm_to_inl_asm(args.input, args.output))
