{
  'type': 'static_library',
  'include_dirs': [
    '<(jsonifier_include_dir)',
    '../../src',
  ],
  'configurations': {
    'Release': {
      'defines': [ 'NDEBUG' ],
    },
  },
}
