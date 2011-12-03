#!/usr/bin/env python

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies build of an executable in three different configurations.
"""

import TestGyp

test = TestGyp.TestGyp(formats=['msvs'])

test.run_gyp('configurations.gyp')

for platform in ['Win32', 'x64']:
  test.set_configuration('Debug|%s' % platform)
  test.build('configurations.gyp', rebuild=True)
  try:
    test.run_built_executable('configurations',
                              stdout=('Running %s\n' % platform))
  except WindowsError, e:
    # Assume the exe is 64-bit if it can't load on 32-bit systems.
    if platform == 'x64' and (e.errno == 193 or '[Error 193]' in str(e)):
      continue
    raise

test.pass_test()
