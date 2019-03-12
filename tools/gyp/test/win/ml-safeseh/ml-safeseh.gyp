# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'ml_safeseh',
      'type': 'executable',
      'sources': [
        'hello.cc',
        'a.asm',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'ImageHasSafeExceptionHandlers': 'true',
        },
        'MASM': {
          'UseSafeExceptionHandlers': 'true',
        },
      },
    },
  ]
}
