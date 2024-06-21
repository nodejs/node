{
  'variables': {
    'nbytes_sources': [ 'nbytes.cpp' ],
  },
  'targets': [
    {
      'target_name': 'nbytes',
      'type': 'static_library',
      'include_dirs': ['.'],
      'direct_dependent_settings': {
        'include_dirs': ['.'],
      },
      'sources': [ '<@(nbytes_sources)' ]
    },
  ]
}
