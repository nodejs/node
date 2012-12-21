# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_incs',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'AdditionalIncludeDirectories': [
            'subdir',
          ],
        }
      },
      'sources': ['additional-include-dirs.cc'],
    },
  ]
}
