# Copyright (c) 2015 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'an_exe',
      'type': 'executable',
      'sources': ['exe.cc'],
      'dependencies': [
        'a_dll',
      ],
    },
    {
      'target_name': 'a_dll',
      'type': 'shared_library',
      'sources': ['dll.cc'],
      'dependencies': [
        'a_lib',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'UseLibraryDependencyInputs': 'true'
        },
      },
    },
    {
      'target_name': 'a_lib',
      'type': 'static_library',
      'dependencies': [
        'a_module',
      ],
      'sources': ['a.cc'],
    },
    {
      'target_name': 'a_module',
      'type': 'loadable_module',
      'sources': ['a.cc'],
    },
  ]
}
