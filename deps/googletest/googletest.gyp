{
  'variables': {
    'googletest_sources': [
      'include/gtest/gtest-assertion-result.h',
      'include/gtest/gtest-death-test.h',
      'include/gtest/gtest-matchers.h',
      'include/gtest/gtest-message.h',
      'include/gtest/gtest-param-test.h',
      'include/gtest/gtest-printers.h',
      'include/gtest/gtest-spi.h',
      'include/gtest/gtest-test-part.h',
      'include/gtest/gtest-typed-test.h',
      'include/gtest/gtest.h',
      'include/gtest/gtest_pred_impl.h',
      'include/gtest/internal/custom/gtest-port.h',
      'include/gtest/internal/custom/gtest-printers.h',
      'include/gtest/internal/custom/gtest.h',
      'include/gtest/internal/gtest-death-test-internal.h',
      'include/gtest/internal/gtest-filepath.h',
      'include/gtest/internal/gtest-internal.h',
      'include/gtest/internal/gtest-param-util.h',
      'include/gtest/internal/gtest-port-arch.h',
      'include/gtest/internal/gtest-port.h',
      'include/gtest/internal/gtest-string.h',
      'include/gtest/internal/gtest-type-util.h',
      'src/gtest-assertion-result.cc',
      'src/gtest-death-test.cc',
      'src/gtest-filepath.cc',
      'src/gtest-internal-inl.h',
      'src/gtest-matchers.cc',
      'src/gtest-port.cc',
      'src/gtest-printers.cc',
      'src/gtest-test-part.cc',
      'src/gtest-typed-test.cc',
      'src/gtest.cc',
    ]
  },
  'targets': [
    {
      'target_name': 'gtest',
      'type': 'static_library',
      'sources': [
        '<@(googletest_sources)',
      ],
      'include_dirs': [
        '.', # src
        'include',
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
          'include',
        ],
      },
    },
    {
      'target_name': 'gtest_main',
      'type': 'static_library',
      'dependencies': [
        'gtest',
      ],
      'sources': [
        'src/gtest_main.cc',
      ],
    },
    {
      'target_name': 'gtest_prod',
      'type': 'none',
      'sources': [
        'include/gtest/gtest_prod.h',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'include',
        ],
      },
    },
  ],
}
