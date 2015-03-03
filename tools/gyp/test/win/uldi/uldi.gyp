# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'lib1',
      'type': 'static_library',
      'sources': ['a.cc'],
    },
    {
      'target_name': 'final_uldi',
      'type': 'executable',
      'dependencies': [
        'lib1',
        'lib2',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'UseLibraryDependencyInputs': 'true'
        },
      },
      'sources': ['main.cc'],
    },
    {
      'target_name': 'final_no_uldi',
      'type': 'executable',
      'dependencies': [
        'lib1',
        'lib2',
      ],
      'sources': ['main.cc'],
    },
    {
      'target_name': 'lib2',
      'type': 'static_library',
      # b.cc has the same named function as a.cc, but don't use the same name
      # so that the .obj will have a different name. If the obj file has the
      # same name, the linker will discard the obj file, invalidating the
      # test.
      'sources': ['b.cc'],
    },
  ]
}
