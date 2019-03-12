#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Tests that filenames that contain colons are handled correctly.
(This is important for absolute paths on Windows.)
"""

from __future__ import print_function

import os
import sys

if sys.platform == 'win32':
  print("This test is currently disabled: https://crbug.com/483696.")
  sys.exit(0)


import TestGyp

# TODO: Make colons in filenames work with make, if required.
test = TestGyp.TestGyp(formats=['!make'])
CHDIR = 'colon'

source_name = 'colon/a:b.c'
copies_name = 'colon/a:b.c-d'
if sys.platform == 'win32':
  # Windows uses : as drive separator and doesn't allow it in regular filenames.
  # Use abspath() to create a path that contains a colon instead.
  abs_source = os.path.abspath('colon/file.c')
  test.write('colon/test.gyp',
             test.read('colon/test.gyp').replace("'a:b.c'", repr(abs_source)))
  source_name = abs_source

  abs_copies = os.path.abspath('colon/file.txt')
  test.write('colon/test.gyp',
             test.read('colon/test.gyp').replace("'a:b.c-d'", repr(abs_copies)))
  copies_name = abs_copies

# Create the file dynamically, Windows is unhappy if a file with a colon in
# its name is checked in.
test.write(source_name, 'int main() {}')
test.write(copies_name, 'foo')

test.run_gyp('test.gyp', chdir=CHDIR)
test.build('test.gyp', test.ALL, chdir=CHDIR)
test.built_file_must_exist(os.path.basename(copies_name), chdir=CHDIR)
test.pass_test()
