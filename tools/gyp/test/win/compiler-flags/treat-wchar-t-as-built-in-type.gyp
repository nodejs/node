# Copyright (c) 2010 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'test_treat_wchar_t_as_built_in_type_negative',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'TreatWChar_tAsBuiltInType': 'false',
        },
      },
      'sources': [
        'treat-wchar-t-as-built-in-type1.cc',
      ],
    },
    {
      'target_name': 'test_treat_wchar_t_as_built_in_type_positive',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'TreatWChar_tAsBuiltInType': 'true',
        },
      },
      'sources': [
        'treat-wchar-t-as-built-in-type2.cc',
      ],
    },

  ],
}
