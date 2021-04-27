{
  'targets': [
    {
      'target_name': 'modp_b64',
      'type': 'static_library',
      'include_dirs': [ '.' ],
      'direct_dependent_settings': {
        'include_dirs': [ '.' ],
      },
      'sources': [ 'modp_b64.cc' ],
    },
  ]
}
