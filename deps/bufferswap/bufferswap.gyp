{
  'variables': {
    'target_arch%': '',
  },
  'targets': [
    {
      'target_name': 'bufferswap',
      'type': 'static_library',
      'include_dirs': [ 'include'],
      'direct_dependent_settings': {
        'include_dirs': [ 'include' ],
        'defines': [ 'BUFFERSWAP_STATIC_DEFINE' ],
      },
      'sources': [ 'include/libbufferswap.h' ],
      'defines': [ 'BUFFERSWAP_STATIC_DEFINE' ],
    },
  ]
}
