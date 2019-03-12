# Copyright (c) 2010 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'test_default_char_is_unsigned',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'DefaultCharIsUnsigned': 'true',
        },
      },
      'sources': [
        'default-char-is-unsigned.cc',
      ],
    },
  ],
}
