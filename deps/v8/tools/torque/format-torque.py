#!/usr/bin/env python
# Copyright 2014 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This program either generates the parser files for Torque, generating
the source and header files directly in V8's src directory."""

import subprocess
import sys
import re
from subprocess import Popen, PIPE

def preprocess(input):
  input = re.sub(r'(if\s+)constexpr(\s*\()', r'\1/*COxp*/\2', input)
  input = re.sub(r'(\)\s*\:\s*\S+\s+)labels\s+',
      r'\1,\n/*_LABELS_HOLD_*/ ', input)
  input = re.sub(r'(\s+)operator\s*(\'[^\']+\')', r'\1/*_OPE \2*/', input)
  input = re.sub(r'(\s+)typeswitch\s*\(', r'\1/*_TYPE*/switch (', input)
  input = re.sub(r'(\s+)case\s*\(([^\s]+)\s+\:\s*([^\:]+)\)(\s*)\:',
      r'\1case \3: /*_TSV\2:*/', input)
  input = re.sub(r'(\s+)case\s*\(([^\:]+)\)(\s*)\:',
      r'\1case \2: /*_TSX*/', input)
  input = re.sub(r'\sgenerates\s+\'([^\']+)\'\s*',
      r' _GeNeRaTeS00_/*\1@*/', input)
  input = re.sub(r'\sconstexpr\s+\'([^\']+)\'\s*',
      r' _CoNsExP_/*\1@*/', input)
  input = re.sub(r'\notherwise',
      r'\n otherwise', input)
  input = re.sub(r'(\n\s*\S[^\n]*\s)otherwise',
      r'\1_OtheSaLi', input)
  return input

def postprocess(output):
  output = re.sub(r'\/\*COxp\*\/', r'constexpr', output)
  output = re.sub(r'(\S+)\s*: type([,>])', r'\1: type\2', output)
  output = re.sub(r',([\n ]*)\/\*_LABELS_HOLD_\*\/', r'\1labels', output)
  output = re.sub(r'\/\*_OPE \'([^\']+)\'\*\/', r"operator '\1'", output)
  output = re.sub(r'\/\*_TYPE\*\/(\s*)switch', r'typeswitch', output)
  output = re.sub(r'case ([^\:]+)\:\s*\/\*_TSX\*\/',
      r'case (\1):', output)
  output = re.sub(r'case ([^\:]+)\:\s*\/\*_TSV([^\:]+)\:\*\/',
      r'case (\2: \1):', output)
  output = re.sub(r'\n_GeNeRaTeS00_\s*\/\*([^@]+)@\*\/',
      r"\n    generates '\1'", output)
  output = re.sub(r'_GeNeRaTeS00_\s*\/\*([^@]+)@\*\/',
      r"generates '\1'", output)
  output = re.sub(r'_CoNsExP_\s*\/\*([^@]+)@\*\/',
      r"constexpr '\1'", output)
  output = re.sub(r'\n(\s+)otherwise',
      r"\n\1    otherwise", output)
  output = re.sub(r'\n(\s+)_OtheSaLi',
      r"\n\1otherwise", output)
  output = re.sub(r'_OtheSaLi',
      r"otherwise", output)
  return output

if len(sys.argv) < 2 or len(sys.argv) > 3:
  print "invalid number of arguments"
  sys.exit(-1)

use_stdout = True
lint = False
if len(sys.argv) == 3:
  if sys.argv[1] == '-i':
    use_stdout = False
  if sys.argv[1] == '-l':
    lint = True

filename = sys.argv[len(sys.argv) - 1]

with open(filename, 'r') as content_file:
  content = content_file.read()
original_input = content
p = Popen(['clang-format', '-assume-filename=.ts'], stdin=PIPE, stdout=PIPE, stderr=PIPE)
output, err = p.communicate(preprocess(content))
output = postprocess(output)
rc = p.returncode
if (rc <> 0):
  sys.exit(rc);
if lint:
  if (output != original_input):
    print >>sys.stderr, filename + ' requires formatting'
elif use_stdout:
  print output
else:
  output_file = open(filename, 'w')
  output_file.write(output);
  output_file.close()
