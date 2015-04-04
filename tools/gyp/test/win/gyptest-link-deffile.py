#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure a .def file is handled in the link.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'linker-flags'

  # Multiple .def files doesn't make any sense, should fail at generate time.
  test.run_gyp('deffile-multiple.gyp', chdir=CHDIR, stderr=None, status=1)

  test.run_gyp('deffile.gyp', chdir=CHDIR)
  test.build('deffile.gyp', test.ALL, chdir=CHDIR)

  def HasExport(binary, export):
    full_path = test.built_file_path(binary, chdir=CHDIR)
    output = test.run_dumpbin('/exports', full_path)
    return export in output

  # Make sure we only have the export when the .def file is in use.

  if HasExport('test_deffile_dll_notexported.dll', 'AnExportedFunction'):
    test.fail_test()
  if not HasExport('test_deffile_dll_ok.dll', 'AnExportedFunction'):
    test.fail_test()

  if HasExport('test_deffile_exe_notexported.exe', 'AnExportedFunction'):
    test.fail_test()
  if not HasExport('test_deffile_exe_ok.exe', 'AnExportedFunction'):
    test.fail_test()

  test.pass_test()
