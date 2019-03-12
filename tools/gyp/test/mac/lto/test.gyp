# Copyright (c) 2015 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'lto',
      'type': 'shared_library',
      'sources': [
        'cfile.c',
        'mfile.m',
        'ccfile.cc',
        'mmfile.mm',
        'asmfile.S',
      ],
      'xcode_settings': {
        'LLVM_LTO': 'YES',
      },
    },
    {
      'target_name': 'lto_static',
      'type': 'static_library',
      'sources': [
        'cfile.c',
        'mfile.m',
        'ccfile.cc',
        'mmfile.mm',
        'asmfile.S',
      ],
      'xcode_settings': {
        'LLVM_LTO': 'YES',
      },
    },
  ],
}
