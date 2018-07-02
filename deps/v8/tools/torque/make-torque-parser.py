#!/usr/bin/env python
# Copyright 2014 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This program either generates the parser files for Torque, generating
the source and header files directly in V8's src directory."""

import subprocess
import sys
import os
import ntpath
import re

cwd = os.getcwd()
tools = ntpath.dirname(sys.argv[0]);
grammar = tools + '/../../src/torque/Torque.g4'
basename = ntpath.basename(grammar)
dirname = ntpath.dirname(grammar)
os.chdir(dirname)
cargs = ['java', '-Xmx500M', 'org.antlr.v4.Tool', '-visitor', basename]
result = subprocess.call(cargs)
os.chdir(cwd)

def fix_file(filename):
  is_header = re.search(r'\.h', filename) <> None;
  header_macro = filename.upper();
  header_macro = re.sub('\.', '_', header_macro);
  header_macro = "V8_TORQUE_" + header_macro + '_';

  copyright = '// Copyright 2018 the V8 project authors. All rights reserved.\n'
  copyright += '// Use of this source code is governed by a BSD-style license that can be\n'
  copyright += '// found in the LICENSE file.\n'
  file_path = tools + '/../../src/torque/' + filename;
  temp_file_path = file_path + '.tmp'
  output_file = open(temp_file_path, 'w')
  output_file.write(copyright);
  if is_header:
    output_file.write('#ifndef ' + header_macro + '\n');
    output_file.write('#define ' + header_macro + '\n');

  with open(file_path) as f:
    content = f.readlines()
  for x in content:
    x = re.sub(';;', ';', x)
    x = re.sub('antlr4-runtime\.h', './antlr4-runtime.h', x)
    x = re.sub('  TorqueParser.antlr4', '  explicit TorqueParser(antlr4', x)
    x = re.sub('  TorqueLexer.antlr4', '  explicit TorqueLexer(antlr4', x)
    if not re.search('= 0', x):
      x = re.sub('virtual', '', x)
    output_file.write(x)

  if is_header:
    output_file.write('#endif  // ' + header_macro + '\n');
  output_file.close();

  subprocess.call(['rm', file_path])
  subprocess.call(['mv', temp_file_path, file_path])

fix_file('TorqueBaseListener.h');
fix_file('TorqueBaseListener.cpp');
fix_file('TorqueBaseVisitor.h');
fix_file('TorqueBaseVisitor.cpp');
fix_file('TorqueLexer.h');
fix_file('TorqueLexer.cpp');
fix_file('TorqueParser.h');
fix_file('TorqueParser.cpp');
fix_file('TorqueListener.h');
fix_file('TorqueListener.cpp');
fix_file('TorqueVisitor.h');
fix_file('TorqueVisitor.cpp');
