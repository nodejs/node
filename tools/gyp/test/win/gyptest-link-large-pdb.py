#!/usr/bin/env python

# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure msvs_large_pdb works correctly.
"""

from __future__ import print_function

import TestGyp

import struct
import sys

if sys.platform == 'win32':
  print("This test is currently disabled: https://crbug.com/483696.")
  sys.exit(0)


CHDIR = 'large-pdb'


def CheckImageAndPdb(test, image_basename, expected_page_size,
                     pdb_basename=None):
  if not pdb_basename:
    pdb_basename = image_basename + '.pdb'
  test.built_file_must_exist(image_basename, chdir=CHDIR)
  test.built_file_must_exist(pdb_basename, chdir=CHDIR)

  # We expect the PDB to have the given page size. For full details of the
  # header look here: https://code.google.com/p/pdbparser/wiki/MSF_Format
  # We read the little-endian 4-byte unsigned integer at position 32 of the
  # file.
  pdb_path = test.built_file_path(pdb_basename, chdir=CHDIR)
  pdb_file = open(pdb_path, 'rb')
  pdb_file.seek(32, 0)
  page_size = struct.unpack('<I', pdb_file.read(4))[0]
  if page_size != expected_page_size:
    print("Expected page size of %d, got %d for PDB file `%s'." % (
        expected_page_size, page_size, pdb_path))


if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  test.run_gyp('large-pdb.gyp', chdir=CHDIR)

  test.build('large-pdb.gyp', 'large_pdb_exe', chdir=CHDIR)
  CheckImageAndPdb(test, 'large_pdb_exe.exe', 4096)

  test.build('large-pdb.gyp', 'small_pdb_exe', chdir=CHDIR)
  CheckImageAndPdb(test, 'small_pdb_exe.exe', 1024)

  test.build('large-pdb.gyp', 'large_pdb_dll', chdir=CHDIR)
  CheckImageAndPdb(test, 'large_pdb_dll.dll', 4096)

  test.build('large-pdb.gyp', 'small_pdb_dll', chdir=CHDIR)
  CheckImageAndPdb(test, 'small_pdb_dll.dll', 1024)

  test.build('large-pdb.gyp', 'large_pdb_implicit_exe', chdir=CHDIR)
  CheckImageAndPdb(test, 'large_pdb_implicit_exe.exe', 4096)

  # This target has a different PDB name because it uses an
  # 'msvs_large_pdb_path' variable.
  test.build('large-pdb.gyp', 'large_pdb_variable_exe', chdir=CHDIR)
  CheckImageAndPdb(test, 'large_pdb_variable_exe.exe', 4096,
                   pdb_basename='foo.pdb')

  # This target has a different output name because it uses 'product_name'.
  test.build('large-pdb.gyp', 'large_pdb_product_exe', chdir=CHDIR)
  CheckImageAndPdb(test, 'bar.exe', 4096)

  test.pass_test()
