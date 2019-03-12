# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'configurations': {
      'Default': {
        'msvs_configuration_platform': 'Win32',
      },
      'Default_x64': {
        'inherit_from': ['Default'],
        'msvs_configuration_platform': 'x64',
      },
    },
  },
  'targets': [
    {
      'target_name': 'test_safeseh_default',
      'type': 'executable',
      'msvs_settings': {
        # By default, msvs passes /SAFESEH for Link, but not for MASM.  In
        # order for test_safeseh_default to link successfully, we need to
        # explicitly specify /SAFESEH for MASM.
        'MASM': {
          'UseSafeExceptionHandlers': 'true',
        },
      },
      'sources': [
        'safeseh_hello.cc',
        'safeseh_zero.asm',
      ],
    },
    {
      'target_name': 'test_safeseh_no',
      'type': 'executable',
      'msvs_settings': {
        'VCLinkerTool': {
          'ImageHasSafeExceptionHandlers': 'false',
        },
      },
      'sources': [
        'safeseh_hello.cc',
        'safeseh_zero.asm',
      ],
    },
    {
      'target_name': 'test_safeseh_yes',
      'type': 'executable',
      'msvs_settings': {
        'VCLinkerTool': {
          'ImageHasSafeExceptionHandlers': 'true',
        },
        'MASM': {
          'UseSafeExceptionHandlers': 'true',
        },
      },
      'sources': [
        'safeseh_hello.cc',
        'safeseh_zero.asm',
      ],
    },
    {
      # x64 targets cannot have ImageHasSafeExceptionHandlers or
      # UseSafeExceptionHandlers set.
      'target_name': 'test_safeseh_x64',
      'type': 'executable',
      'configurations': {
        'Default': {
          'msvs_target_platform': 'x64',
        },
      },
      'sources': [
        'safeseh_hello.cc',
        'safeseh_zero64.asm',
      ],
    },
  ]
}
