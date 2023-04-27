{
  'variables': {
    'histogram_sources': [
      'src/hdr_histogram.c',
      'include/hdr/hdr_histogram.h',
    ]
  },
  'targets': [
    {
      'target_name': 'histogram',
      'type': 'static_library',
      'cflags': ['-fvisibility=hidden'],
      'xcode_settings': {
        'GCC_SYMBOLS_PRIVATE_EXTERN': 'YES',  # -fvisibility=hidden
      },
      'include_dirs': ['src', 'include'],
      'direct_dependent_settings': {
        'include_dirs': [ 'src', 'include' ]
      },
      'sources': [
        '<@(histogram_sources)',
      ]
    }
  ]
}
