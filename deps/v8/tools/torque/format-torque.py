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
  input = re.sub(r'(\s+)operator\s*(\'[^\']+\')', r'\1/*_OPE \2*/', input)

  # Mangle typeswitches to look like switch statements with the extra type
  # information and syntax encoded in comments.
  input = re.sub(r'(\s+)typeswitch\s*\(', r'\1/*_TYPE*/switch (', input)
  input = re.sub(r'(\s+)case\s*\(\s*([^\:]+)\s*\)(\s*)\:\s*deferred',
      r'\1case \2: /*_TSXDEFERRED_*/', input)
  input = re.sub(r'(\s+)case\s*\(\s*([^\:]+)\s*\)(\s*)\:',
      r'\1case \2: /*_TSX*/', input)
  input = re.sub(r'(\s+)case\s*\(\s*([^\s]+)\s*\:\s*([^\:]+)\s*\)(\s*)\:\s*deferred',
      r'\1case \3: /*_TSVDEFERRED_\2:*/', input)
  input = re.sub(r'(\s+)case\s*\(\s*([^\s]+)\s*\:\s*([^\:]+)\s*\)(\s*)\:',
      r'\1case \3: /*_TSV\2:*/', input)

  # Add extra space around | operators to fix union types later.
  while True:
    old = input
    input = re.sub(r'(\w+\s*)\|(\s*\w+)',
        r'\1|/**/\2', input)
    if old == input:
      break;

  input = re.sub(r'\bgenerates\s+\'([^\']+)\'\s*',
      r' _GeNeRaTeS00_/*\1@*/', input)
  input = re.sub(r'\bconstexpr\s+\'([^\']+)\'\s*',
      r' _CoNsExP_/*\1@*/', input)
  input = re.sub(r'\notherwise',
      r'\n otherwise', input)
  input = re.sub(r'(\n\s*\S[^\n]*\s)otherwise',
      r'\1_OtheSaLi', input)
  return input

def postprocess(output):
  output = re.sub(r'% RawObjectCast', r'%RawObjectCast', output)
  output = re.sub(r'% RawPointerCast', r'%RawPointerCast', output)
  output = re.sub(r'% RawConstexprCast', r'%RawConstexprCast', output)
  output = re.sub(r'% FromConstexpr', r'%FromConstexpr', output)
  output = re.sub(r'% Allocate', r'%Allocate', output)
  output = re.sub(r'\/\*COxp\*\/', r'constexpr', output)
  output = re.sub(r'(\S+)\s*: type([,>])', r'\1: type\2', output)
  output = re.sub(r'(\n\s*)labels( [A-Z])', r'\1    labels\2', output)
  output = re.sub(r'\/\*_OPE \'([^\']+)\'\*\/', r"operator '\1'", output)
  output = re.sub(r'\/\*_TYPE\*\/(\s*)switch', r'typeswitch', output)
  output = re.sub(r'case (\w+)\:\s*\/\*_TSXDEFERRED_\*\/',
      r'case (\1): deferred', output)
  output = re.sub(r'case (\w+)\:\s*\/\*_TSX\*\/',
      r'case (\1):', output)
  output = re.sub(r'case (\w+)\:\s*\/\*_TSVDEFERRED_([^\:]+)\:\*\/',
      r'case (\2: \1): deferred', output)
  output = re.sub(r'case (\w+)\:\s*\/\*_TSV([^\:]+)\:\*\/',
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

  while True:
    old = output
    output = re.sub(r'(\w+)\s{0,1}\|\s{0,1}/\*\*/(\s*\w+)',
        r'\1 |\2', output)
    if old == output:
      break;

  return output

def process(filename, only_lint, use_stdout):
  with open(filename, 'r') as content_file:
    content = content_file.read()

  original_input = content

  p = Popen(['clang-format', '-assume-filename=.ts'], stdin=PIPE, stdout=PIPE, stderr=PIPE)
  output, err = p.communicate(preprocess(content))
  output = postprocess(output)
  rc = p.returncode
  if (rc <> 0):
    print "error code " + str(rc) + " running clang-format. Exiting..."
    sys.exit(rc);

  if only_lint:
    if (output != original_input):
      print >>sys.stderr, filename + ' requires formatting'
  elif use_stdout:
    print output
  else:
    output_file = open(filename, 'w')
    output_file.write(output);
    output_file.close()

def print_usage():
  print 'format-torque -i file1[, file2[, ...]]'
  print '    format and overwrite input files'
  print 'format-torque -l file1[, file2[, ...]]'
  print '    merely indicate which files need formatting'

def Main():
  if len(sys.argv) < 3:
    print "error: at least 2 arguments required"
    print_usage();
    sys.exit(-1)

  use_stdout = True
  lint = False

  if sys.argv[1] == '-i':
    use_stdout = False
  elif sys.argv[1] == '-l':
    lint = True
  else:
    print "error: -i or -l must be specified as the first argument"
    print_usage();
    sys.exit(-1);

  for filename in sys.argv[2:]:
    process(filename, lint, use_stdout)

  return 0

if __name__ == '__main__':
  sys.exit(Main());
