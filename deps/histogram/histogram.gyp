{
  'targets': [
    {
      'target_name': 'histogram',
      'type': 'static_library',
      'include_dirs': ['src'],
      'direct_dependent_settings': {
        'include_dirs': [ 'src' ]
      },
      'sources': [
        'src/hdr_histogram.c',
      ]
    }
  ]
}
