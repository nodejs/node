# Copyright (c) 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'test_midl_include_dirs',
      'type': 'executable',
      'sources': [
        'hello.cc',
        'subdir/foo.idl',
        'subdir/bar.idl',
      ],
      'midl_include_dirs': [
        'subdir',
      ],
      'msvs_settings': {
        'VCMIDLTool': {
          'OutputDirectory': '<(INTERMEDIATE_DIR)',
          'DLLDataFileName': '$(InputName)_dlldata.h',
         },
      },
    },
  ],
}
