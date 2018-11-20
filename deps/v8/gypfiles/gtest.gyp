# Copyright 2014 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'gtest',
      'toolsets': ['host', 'target'],
      'type': 'static_library',
      'sources': [
        '../testing/gtest/include/gtest/gtest-death-test.h',
        '../testing/gtest/include/gtest/gtest-message.h',
        '../testing/gtest/include/gtest/gtest-param-test.h',
        '../testing/gtest/include/gtest/gtest-printers.h',
        '../testing/gtest/include/gtest/gtest-spi.h',
        '../testing/gtest/include/gtest/gtest-test-part.h',
        '../testing/gtest/include/gtest/gtest-typed-test.h',
        '../testing/gtest/include/gtest/gtest.h',
        '../testing/gtest/include/gtest/gtest_pred_impl.h',
        '../testing/gtest/include/gtest/internal/gtest-death-test-internal.h',
        '../testing/gtest/include/gtest/internal/gtest-filepath.h',
        '../testing/gtest/include/gtest/internal/gtest-internal.h',
        '../testing/gtest/include/gtest/internal/gtest-linked_ptr.h',
        '../testing/gtest/include/gtest/internal/gtest-param-util-generated.h',
        '../testing/gtest/include/gtest/internal/gtest-param-util.h',
        '../testing/gtest/include/gtest/internal/gtest-port.h',
        '../testing/gtest/include/gtest/internal/gtest-string.h',
        '../testing/gtest/include/gtest/internal/gtest-tuple.h',
        '../testing/gtest/include/gtest/internal/gtest-type-util.h',
        '../testing/gtest/src/gtest-all.cc',
        '../testing/gtest/src/gtest-death-test.cc',
        '../testing/gtest/src/gtest-filepath.cc',
        '../testing/gtest/src/gtest-internal-inl.h',
        '../testing/gtest/src/gtest-port.cc',
        '../testing/gtest/src/gtest-printers.cc',
        '../testing/gtest/src/gtest-test-part.cc',
        '../testing/gtest/src/gtest-typed-test.cc',
        '../testing/gtest/src/gtest.cc',
        '../testing/gtest-support.h',
      ],
      'sources!': [
        '../testing/gtest/src/gtest-all.cc',  # Not needed by our build.
      ],
      'include_dirs': [
        '../testing/gtest',
        '../testing/gtest/include',
      ],
      'dependencies': [
        'gtest_prod',
      ],
      'defines': [
        # In order to allow regex matches in gtest to be shared between Windows
        # and other systems, we tell gtest to always use it's internal engine.
        'GTEST_HAS_POSIX_RE=0',
        'GTEST_LANG_CXX11=1',
      ],
      'all_dependent_settings': {
        'defines': [
          'GTEST_HAS_POSIX_RE=0',
          'GTEST_LANG_CXX11=1',
        ],
      },
      'conditions': [
        ['OS=="android"', {
          'defines': [
            'GTEST_HAS_CLONE=0',
          ],
          'direct_dependent_settings': {
            'defines': [
              'GTEST_HAS_CLONE=0',
            ],
          },
        }],
      ],
      'direct_dependent_settings': {
        'defines': [
          'UNIT_TEST',
        ],
        'include_dirs': [
          '../testing/gtest/include',  # So that gtest headers can find themselves.
        ],
        'target_conditions': [
          ['_type=="executable"', {
            'test': 1,
            'conditions': [
              ['OS=="mac"', {
                'run_as': {
                  'action????': ['${BUILT_PRODUCTS_DIR}/${PRODUCT_NAME}'],
                },
              }],
              ['OS=="win"', {
                'run_as': {
                  'action????': ['$(TargetPath)', '--gtest_print_time'],
                },
              }],
            ],
          }],
        ],
        'msvs_disabled_warnings': [4800],
      },
    },
    {
      'target_name': 'gtest_main',
      'type': 'static_library',
      'dependencies': [
        'gtest',
      ],
      'sources': [
        '../testing/gtest/src/gtest_main.cc',
      ],
    },
    {
      'target_name': 'gtest_prod',
      'toolsets': ['host', 'target'],
      'type': 'none',
      'sources': [
        '../testing/gtest/include/gtest/gtest_prod.h',
      ],
    },
  ],
}
