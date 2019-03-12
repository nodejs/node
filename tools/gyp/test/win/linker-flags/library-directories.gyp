# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_libdirs_none',
      'type': 'executable',
      'msvs_settings': {
        'VCLinkerTool': {
          'AdditionalDependencies': [
            'test_lib.lib',
          ],
        },
      },
      'sources': ['library-directories-reference.cc'],
    },
    {
      'target_name': 'test_libdirs_with',
      'type': 'executable',
      'msvs_settings': {
        'VCLinkerTool': {
          # NOTE: Don't use this for general dependencies between gyp
          # libraries (use 'dependencies' instead). This is done here only for
          # testing.
          #
          # This setting should only be used to depend on third party prebuilt
          # libraries that are stored as binaries at a known location.
          'AdditionalLibraryDirectories': [
            '<(DEPTH)/out/Default/obj/subdir', # ninja style
            '<(DEPTH)/subdir/Default/lib', # msvs style
          ],
          'AdditionalDependencies': [
            'test_lib.lib',
          ],
        },
      },
      'sources': ['library-directories-reference.cc'],
    },
  ]
}
