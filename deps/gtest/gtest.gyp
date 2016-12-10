{
  'targets': [
    {
      'target_name': 'gtest',
      'type': 'static_library',
      'cflags': ['-Wno-missing-field-initializers'],
      'direct_dependent_settings': {
        'include_dirs': ['include'],
      },
      'include_dirs': ['.', 'include'],
      'sources': [
        'src/gtest-death-test.cc',
        'src/gtest-filepath.cc',
        'src/gtest-internal-inl.h',
        'src/gtest-port.cc',
        'src/gtest-printers.cc',
        'src/gtest-test-part.cc',
        'src/gtest-typed-test.cc',
        'src/gtest.cc',
        'src/gtest_main.cc',
      ],
    }
  ],
}
