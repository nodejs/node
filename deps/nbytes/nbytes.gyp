{
  'variables': {
    'nbytes_sources': [ 'src/nbytes.cpp' ],
  },
  'targets': [
    {
      'target_name': 'nbytes',
      'type': 'static_library',
      'include_dirs': ['src', 'include'],
      'direct_dependent_settings': {
        'include_dirs': ['src', 'include'],
      },
      'sources': [ '<@(nbytes_sources)' ]
    },
  ]
}
