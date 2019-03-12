# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_on',
      'type': 'executable',
      'msvs_settings': {
        'VCLinkerTool': {
          'TreatLinkerWarningAsErrors': 'true',
        }
      },
      'sources': ['link-warning.cc'],
    },
    {
      'target_name': 'test_off',
      'type': 'executable',
      'msvs_settings': {
        'VCLinkerTool': {
          'TreatLinkerWarningAsErrors': 'false',
        }
      },
      'sources': ['link-warning.cc'],
    },
    {
      'target_name': 'test_default',
      'type': 'executable',
      'sources': ['link-warning.cc'],
    },
  ]
}
