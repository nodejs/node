# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'target_conditions': [
      ['_toolset=="target"', {'defines': ['TARGET']}]
    ]
  },
  'targets': [
    {
      'target_name': 'toolsets',
      'type': 'static_library',
      'toolsets': ['target', 'host'],
      'sources': [
        'toolsets.cc',
      ],
    },
    {
      'target_name': 'host-main',
      'type': 'executable',
      'toolsets': ['host'],
      'dependencies': ['toolsets', 'toolsets_shared'],
      'sources': [
        'main.cc',
      ],
    },
    {
      'target_name': 'target-main',
      'type': 'executable',
      'dependencies': ['toolsets', 'toolsets_shared'],
      'sources': [
        'main.cc',
      ],
    },
    # This tests that build systems can handle a shared library being build for
    # both host and target.
    {
      'target_name': 'janus',
      'type': 'shared_library',
      'toolsets': ['target', 'host'],
      'sources': [
        'toolsets.cc',
      ],
      'cflags': [ '-fPIC' ],
    },
    {
      'target_name': 'toolsets_shared',
      'type': 'shared_library',
      'toolsets': ['target', 'host'],
      'target_conditions': [
        # Ensure target and host have different shared_library names
        ['_toolset=="host"', {'product_extension': 'host'}],
      ],
      'sources': [
        'toolsets_shared.cc',
      ],
      'cflags': [ '-fPIC' ],
    },
  ],
}
