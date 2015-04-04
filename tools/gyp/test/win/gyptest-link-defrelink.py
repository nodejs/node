#!/usr/bin/env python

# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure a relink is performed when a .def file is touched.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'linker-flags'
  target = 'test_deffile_dll_ok'
  def_contents = test.read('linker-flags/deffile.def')

  # This first build makes sure everything is up to date.
  test.run_gyp('deffile.gyp', chdir=CHDIR)
  test.build('deffile.gyp', target, chdir=CHDIR)
  test.up_to_date('deffile.gyp', target, chdir=CHDIR)

  def HasExport(binary, export):
    full_path = test.built_file_path(binary, chdir=CHDIR)
    output = test.run_dumpbin('/exports', full_path)
    return export in output

  # Verify that only one function is exported.
  if not HasExport('test_deffile_dll_ok.dll', 'AnExportedFunction'):
    test.fail_test()
  if HasExport('test_deffile_dll_ok.dll', 'AnotherExportedFunction'):
    test.fail_test()

  # Add AnotherExportedFunction to the def file, then rebuild.  If it doesn't
  # relink the DLL, then the subsequent check for AnotherExportedFunction will
  # fail.
  new_def_contents = def_contents + "\n    AnotherExportedFunction"
  test.write('linker-flags/deffile.def', new_def_contents)
  test.build('deffile.gyp', target, chdir=CHDIR)
  test.up_to_date('deffile.gyp', target, chdir=CHDIR)

  if not HasExport('test_deffile_dll_ok.dll', 'AnExportedFunction'):
    test.fail_test()
  if not HasExport('test_deffile_dll_ok.dll', 'AnotherExportedFunction'):
    test.fail_test()

  test.pass_test()
