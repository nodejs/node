# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'target_defaults': {
    'xcode_settings': {
      'GCC_TREAT_WARNINGS_AS_ERRORS': 'YES',
    },
  },

  'variables': {
    # Non-failing tests should check that these trivial files in every language
    # still compile correctly.
    'valid_sources': [
      'valid_c.c',
      'valid_cc.cc',
      'valid_m.m',
      'valid_mm.mm',
    ],
  },

  # Targets come in pairs: 'foo' and 'foo-fail', with the former building with
  # no warnings and the latter not.
  'targets': [
    # GCC_WARN_ABOUT_INVALID_OFFSETOF_MACRO (default: YES):
    {
      'target_name': 'warn_about_invalid_offsetof_macro',
      'type': 'executable',
      'sources': [
        'warn_about_invalid_offsetof_macro.cc',
        '<@(valid_sources)',
      ],
      'xcode_settings': {
        'GCC_WARN_ABOUT_INVALID_OFFSETOF_MACRO': 'NO',
      },
    },
    {
      'target_name': 'warn_about_invalid_offsetof_macro-fail',
      'type': 'executable',
      'sources': [ 'warn_about_invalid_offsetof_macro.cc', ],
    },
    # GCC_WARN_ABOUT_MISSING_NEWLINE (default: NO):
    {
      'target_name': 'warn_about_missing_newline',
      'type': 'executable',
      'sources': [
        'warn_about_missing_newline.c',
        '<@(valid_sources)',
      ],
    },
    {
      'target_name': 'warn_about_missing_newline-fail',
      'type': 'executable',
      'sources': [ 'warn_about_missing_newline.c', ],
      'xcode_settings': {
        'GCC_WARN_ABOUT_MISSING_NEWLINE': 'YES',
      },
    },
  ],
}
