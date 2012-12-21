#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that filenames passed to various linker flags are converted into
build-directory relative paths correctly.
"""

import TestGyp

import sys

if sys.platform == 'darwin':
  test = TestGyp.TestGyp(formats=['ninja', 'make', 'xcode'])

  CHDIR = 'ldflags'
  test.run_gyp('subdirectory/test.gyp', chdir=CHDIR)

  test.build('subdirectory/test.gyp', test.ALL, chdir=CHDIR)

  test.pass_test()


# These flags from `man ld` couldl show up in OTHER_LDFLAGS and need path
# translation.
#
# Done:
#      -exported_symbols_list filename
#      -unexported_symbols_list file
#      -reexported_symbols_list file
#      -sectcreate segname sectname file
#
# Will be done on demand:
#      -weak_library path_to_library
#      -reexport_library path_to_library
#      -lazy_library path_to_library
#      -upward_library path_to_library
#      -syslibroot rootdir
#      -framework name[,suffix]
#      -weak_framework name[,suffix]
#      -reexport_framework name[,suffix]
#      -lazy_framework name[,suffix]
#      -upward_framework name[,suffix]
#      -force_load path_to_archive
#      -filelist file[,dirname]
#      -dtrace file
#      -order_file file                     # should use ORDER_FILE
#      -exported_symbols_order file
#      -bundle_loader executable            # should use BUNDLE_LOADER
#      -alias_list filename
#      -seg_addr_table filename
#      -dylib_file install_name:file_name
#      -interposable_list filename
#      -object_path_lto filename
#
#
# obsolete:
#      -sectorder segname sectname orderfile
#      -seg_addr_table_filename path
#
#
# ??:
#      -map map_file_path
#      -sub_library library_name
#      -sub_umbrella framework_name
