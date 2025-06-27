{
  'variables': {
    'llhttp_sources': [
      'src/llhttp.c',
      'src/api.c',
      'src/http.c',
    ]
  },
  'targets': [
    {
      'target_name': 'llhttp',
      'type': 'static_library',
      'include_dirs': [ '.', 'include' ],
      'direct_dependent_settings': {
        'include_dirs': [ 'include' ],
      },
      'sources': [
        '<@(llhttp_sources)',
      ],
    },
  ]
}
