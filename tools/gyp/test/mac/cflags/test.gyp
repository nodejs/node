# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'mytarget',
      'type': 'shared_library',
      'sources': [
        'cfile.c',
        'mfile.m',
        'ccfile.cc',
        'cppfile.cpp',
        'cxxfile.cxx',
        'mmfile.mm',
      ],
      'xcode_settings': {
        # Normally, defines would go in 'defines' instead. This is just for
        # testing.
        'OTHER_CFLAGS': [
          '-DCFLAG',
        ],
        'OTHER_CPLUSPLUSFLAGS': [
          '-DCCFLAG',
        ],
        'GCC_C_LANGUAGE_STANDARD': 'c99',
      },
    },
    {
      'target_name': 'mytarget_reuse_cflags',
      'type': 'shared_library',
      'sources': [
        'cfile.c',
        'mfile.m',
        'ccfile_withcflags.cc',
        'cppfile_withcflags.cpp',
        'cxxfile_withcflags.cxx',
        'mmfile_withcflags.mm',
      ],
      'xcode_settings': {
        'OTHER_CFLAGS': [
          '-DCFLAG',
        ],
        'OTHER_CPLUSPLUSFLAGS': [
          '$OTHER_CFLAGS',
          '-DCCFLAG',
        ],
        # This is a C-only flag, to check these don't get added to C++ files.
        'GCC_C_LANGUAGE_STANDARD': 'c99',
      },
    },
    {
      'target_name': 'mytarget_inherit_cflags',
      'type': 'shared_library',
      'sources': [
        'cfile.c',
        'mfile.m',
        'ccfile_withcflags.cc',
        'cppfile_withcflags.cpp',
        'cxxfile_withcflags.cxx',
        'mmfile_withcflags.mm',
      ],
      'xcode_settings': {
        'OTHER_CFLAGS': [
          '-DCFLAG',
        ],
        'OTHER_CPLUSPLUSFLAGS': [
          '$inherited',
          '-DCCFLAG',
        ],
        'GCC_C_LANGUAGE_STANDARD': 'c99',
      },
    },
    {
      'target_name': 'mytarget_inherit_cflags_parens',
      'type': 'shared_library',
      'sources': [
        'cfile.c',
        'mfile.m',
        'ccfile_withcflags.cc',
        'cppfile_withcflags.cpp',
        'cxxfile_withcflags.cxx',
        'mmfile_withcflags.mm',
      ],
      'xcode_settings': {
        'OTHER_CFLAGS': [
          '-DCFLAG',
        ],
        'OTHER_CPLUSPLUSFLAGS': [
          '$(inherited)',
          '-DCCFLAG',
        ],
        'GCC_C_LANGUAGE_STANDARD': 'c99',
      },
    },
    {
      'target_name': 'mytarget_inherit_cflags_braces',
      'type': 'shared_library',
      'sources': [
        'cfile.c',
        'mfile.m',
        'ccfile_withcflags.cc',
        'cppfile_withcflags.cpp',
        'cxxfile_withcflags.cxx',
        'mmfile_withcflags.mm',
      ],
      'xcode_settings': {
        'OTHER_CFLAGS': [
          '-DCFLAG',
        ],
        'OTHER_CPLUSPLUSFLAGS': [
          '${inherited}',
          '-DCCFLAG',
        ],
        'GCC_C_LANGUAGE_STANDARD': 'c99',
      },
    },
    {
      'target_name': 'ansi_standard',
      'type': 'shared_library',
      'sources': [
        'cfile.c',
      ],
      'xcode_settings': {
        'OTHER_CFLAGS': [
          '-DCFLAG',
        ],
        'GCC_C_LANGUAGE_STANDARD': 'ansi',
      },
    },
  ],
}
