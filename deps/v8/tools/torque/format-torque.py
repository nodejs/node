#!/usr/bin/env python
# -*- coding: utf-8 -*-
# Copyright 2014 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This program either generates the parser files for Torque, generating
the source and header files directly in V8's src directory."""

# for py2/py3 compatibility
from __future__ import print_function

import subprocess
import sys
import re
from subprocess import Popen, PIPE

kPercentEscape = r'α';  # Unicode alpha

def preprocess(input):
  input = re.sub(r'(if\s+)constexpr(\s*\()', r'\1/*COxp*/\2', input)
  input = re.sub(r'(\s+)operator\s*(\'[^\']+\')', r'\1/*_OPE \2*/', input)
  input = re.sub(r'\btypeswitch\s*(\([^{]*\))\s{', r' if /*tPsW*/ \1 {', input)
  input = re.sub(r'\bcase\s*(\([^{]*\))\s*:\s*deferred\s*{', r' if /*cAsEdEfF*/ \1 {', input)
  input = re.sub(r'\bcase\s*(\([^{]*\))\s*:\s*{', r' if /*cA*/ \1 {', input)

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
  input = re.sub(r'@if\(', r'@iF(', input)
  input = re.sub(r'@export', r'@eXpOrT', input)
  input = re.sub(r'js-implicit[ \n]+', r'jS_iMpLiCiT_', input)

  # Special handing of '%' for intrinsics, turn the percent
  # into a unicode character so that it gets treated as part of the
  # intrinsic's name if it's already adjacent to it.
  input = re.sub(r'%([A-Za-z])', kPercentEscape + r'\1', input)

  return input

def postprocess(output):
  output = re.sub(r'\/\*COxp\*\/', r'constexpr', output)
  output = re.sub(r'(\S+)\s*: type([,>])', r'\1: type\2', output)
  output = re.sub(r'(\n\s*)labels( [A-Z])', r'\1    labels\2', output)
  output = re.sub(r'\/\*_OPE \'([^\']+)\'\*\/', r"operator '\1'", output)
  output = re.sub(r'\bif\s*\/\*tPsW\*\/', r'typeswitch', output)
  output = re.sub(r'\bif\s*\/\*cA\*\/\s*(\([^{]*\))\s*{', r'case \1: {', output)
  output = re.sub(r'\bif\s*\/\*cAsEdEfF\*\/\s*(\([^{]*\))\s*{', r'case \1: deferred {', output)
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
  output = re.sub(r'@iF\(', r'@if(', output)
  output = re.sub(r'@eXpOrT',
      r"@export", output)
  output = re.sub(r'jS_iMpLiCiT_',
      r"js-implicit ", output)

  while True:
    old = output
    output = re.sub(r'(\w+)\s{0,1}\|\s{0,1}/\*\*/(\s*\w+)',
        r'\1 |\2', output)
    if old == output:
      break;

  output = re.sub(kPercentEscape, r'%', output)

  return output

def process(filename, lint, should_format):
  with open(filename, 'r') as content_file:
    content = content_file.read()

  original_input = content

  if sys.platform.startswith('win'):
    p = Popen(['clang-format', '-assume-filename=.ts'], stdin=PIPE, stdout=PIPE, stderr=PIPE, shell=True)
  else:
    p = Popen(['clang-format', '-assume-filename=.ts'], stdin=PIPE, stdout=PIPE, stderr=PIPE)
  output, err = p.communicate(preprocess(content))
  output = postprocess(output)
  rc = p.returncode
  if (rc != 0):
    print("error code " + str(rc) + " running clang-format. Exiting...")
    sys.exit(rc);

  if (output != original_input):
    if lint:
      print(filename + ' requires formatting', file=sys.stderr)

    if should_format:
      output_file = open(filename, 'w')
      output_file.write(output);
      output_file.close()

def print_usage():
  print('format-torque -i file1[, file2[, ...]]')
  print('    format and overwrite input files')
  print('format-torque -l file1[, file2[, ...]]')
  print('    merely indicate which files need formatting')

def Main():
  if len(sys.argv) < 3:
    print("error: at least 2 arguments required")
    print_usage();
    sys.exit(-1)

  def is_option(arg):
    return arg in ['-i', '-l', '-il']

  should_format = lint = False
  use_stdout = True

  flag, files = sys.argv[1], sys.argv[2:]
  if is_option(flag):
    if '-i' == flag:
      should_format = True
    elif '-l' == flag:
      lint = True
    else:
      lint = True
      should_format = True
  else:
    print("error: -i and/or -l flags must be specified")
    print_usage();
    sys.exit(-1);

  for filename in files:
    process(filename, lint, should_format)

  return 0

if __name__ == '__main__':
  sys.exit(Main());
