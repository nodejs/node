{
  'variables': {
    'ada_sources': [ 'ada.cpp' ],
  },
  'targets': [
    {
      'target_name': 'ada',
      'type': 'static_library',
      'include_dirs': [
        '.',
      ],
      'direct_dependent_settings': {
        'include_dirs': ['.'],
      },
      'defines': [
        'ADA_USE_SIMDUTF=1',
      ],
      'dependencies': [
        '../../tools/v8_gypfiles/simdutf.gyp:simdutf',
      ],
      'sources': [ '<@(ada_sources)' ]
    },
  ]
}
