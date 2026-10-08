{
  'includes': ['toolchain.gypi'],
  'targets': [
    {
      'target_name': 'simdutf',
      'type': 'static_library',
      'toolsets': ['host', 'target'],
      'variables': {
        'SIMDUTF_ROOT': '../../deps/v8/third_party/simdutf',
      },
      'all_dependent_settings': {
        'include_dirs': [
          '<(SIMDUTF_ROOT)',
        ],
      },
      'include_dirs': ['.'],
      'sources': [
        '<(SIMDUTF_ROOT)/simdutf.cpp',
      ],
    },  # simdutf
  ],
}
