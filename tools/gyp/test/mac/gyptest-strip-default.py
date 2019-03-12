#!/usr/bin/env python

# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that the default STRIP_STYLEs match between different generators.
"""

from __future__ import print_function

import TestGyp

import re
import subprocess
import sys
import time

if sys.platform == 'darwin':
  test = TestGyp.TestGyp(formats=['ninja', 'make', 'xcode'])

  CHDIR='strip'
  test.run_gyp('test-defaults.gyp', chdir=CHDIR)

  test.build('test-defaults.gyp', test.ALL, chdir=CHDIR)

  # Lightweight check if stripping was done.
  def OutPath(s):
    return test.built_file_path(s, chdir=CHDIR)

  def CheckNsyms(p, o_expected):
    proc = subprocess.Popen(['nm', '-aU', p], stdout=subprocess.PIPE)
    o = proc.communicate()[0].decode('utf-8')

    # Filter out mysterious "00 0000   OPT radr://5614542" symbol which
    # is apparently only printed on the bots (older toolchain?).
    # Yes, "radr", not "rdar".
    o = ''.join(filter(lambda s: 'radr://5614542' not in s, o.splitlines(True)))

    o = o.replace('A', 'T')
    o = re.sub(r'^[a-fA-F0-9]+', 'XXXXXXXX', o, flags=re.MULTILINE)
    assert not proc.returncode
    if o != o_expected:
      print('Stripping: Expected symbols """\n%s""", got """\n%s"""' % (
          o_expected, o))
      test.fail_test()

  CheckNsyms(OutPath('libsingle_dylib.dylib'),
"""\
XXXXXXXX S _ci
XXXXXXXX S _i
XXXXXXXX T _the_function
XXXXXXXX t _the_hidden_function
XXXXXXXX T _the_used_function
XXXXXXXX T _the_visible_function
""")
  CheckNsyms(OutPath('single_so.so'),
"""\
XXXXXXXX S _ci
XXXXXXXX S _i
XXXXXXXX T _the_function
XXXXXXXX t _the_hidden_function
XXXXXXXX T _the_used_function
XXXXXXXX T _the_visible_function
""")
  CheckNsyms(OutPath('single_exe'),
"""\
XXXXXXXX T __mh_execute_header
""")

  CheckNsyms(test.built_file_path(
      'bundle_dylib.framework/Versions/A/bundle_dylib', chdir=CHDIR),
"""\
XXXXXXXX S _ci
XXXXXXXX S _i
XXXXXXXX T _the_function
XXXXXXXX t _the_hidden_function
XXXXXXXX T _the_used_function
XXXXXXXX T _the_visible_function
""")
  CheckNsyms(test.built_file_path(
      'bundle_so.bundle/Contents/MacOS/bundle_so', chdir=CHDIR),
"""\
XXXXXXXX S _ci
XXXXXXXX S _i
XXXXXXXX T _the_function
XXXXXXXX T _the_used_function
XXXXXXXX T _the_visible_function
""")
  CheckNsyms(test.built_file_path(
      'bundle_exe.app/Contents/MacOS/bundle_exe', chdir=CHDIR),
"""\
XXXXXXXX T __mh_execute_header
""")

  test.pass_test()
