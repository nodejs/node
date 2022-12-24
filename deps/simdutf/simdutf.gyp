{
  'targets': [
    {
      'target_name': 'simdutf',
      'type': 'static_library',
      'include_dirs': ['.'],
      'direct_dependent_settings': {
        'include_dirs': ['.'],
      },
      'sources': ['simdutf.cpp'],
    },
  ]
}
