# Copyright 2014 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'gmock',
      'type': 'static_library',
      'dependencies': [
        'gtest.gyp:gtest',
      ],
      'sources': [
        # Sources based on files in r173 of gmock.
        '../testing/gmock/include/gmock/gmock-actions.h',
        '../testing/gmock/include/gmock/gmock-cardinalities.h',
        '../testing/gmock/include/gmock/gmock-generated-actions.h',
        '../testing/gmock/include/gmock/gmock-generated-function-mockers.h',
        '../testing/gmock/include/gmock/gmock-generated-matchers.h',
        '../testing/gmock/include/gmock/gmock-generated-nice-strict.h',
        '../testing/gmock/include/gmock/gmock-matchers.h',
        '../testing/gmock/include/gmock/gmock-spec-builders.h',
        '../testing/gmock/include/gmock/gmock.h',
        '../testing/gmock/include/gmock/internal/gmock-generated-internal-utils.h',
        '../testing/gmock/include/gmock/internal/gmock-internal-utils.h',
        '../testing/gmock/include/gmock/internal/gmock-port.h',
        '../testing/gmock/src/gmock-all.cc',
        '../testing/gmock/src/gmock-cardinalities.cc',
        '../testing/gmock/src/gmock-internal-utils.cc',
        '../testing/gmock/src/gmock-matchers.cc',
        '../testing/gmock/src/gmock-spec-builders.cc',
        '../testing/gmock/src/gmock.cc',
        '../testing/gmock-support.h',  # gMock helpers
        '../testing/gmock_custom/gmock/internal/custom/gmock-port.h',
      ],
      'sources!': [
        '../testing/gmock/src/gmock-all.cc',  # Not needed by our build.
      ],
      'include_dirs': [
        '../testing/gmock_custom',
        '../testing/gmock',
        '../testing/gmock/include',
      ],
      'all_dependent_settings': {
        'include_dirs': [
          '../testing/gmock_custom',
          '../testing/gmock/include',  # So that gmock headers can find themselves.
        ],
      },
      'export_dependent_settings': [
        'gtest.gyp:gtest',
      ],
      'conditions': [
        ['want_separate_host_toolset==1', {
          'toolsets': ['host', 'target'],
        }, {
          'toolsets': ['target'],
        }],
      ],
    },
    {
      'target_name': 'gmock_main',
      'type': 'static_library',
      'dependencies': [
        'gmock',
      ],
      'sources': [
        '../testing/gmock/src/gmock_main.cc',
      ],
    },
  ],
}
