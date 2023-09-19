{
  'variables': {
    'simdutf_sources': [
      'simdutf.cpp',
    ]
  },
  'targets': [
    {
      'target_name': 'simdutf',
      'toolsets': ['host', 'target'],
      'type': 'static_library',
      'include_dirs': ['.'],
      'direct_dependent_settings': {
        'include_dirs': ['.'],
      },
      'sources': [
        '<@(simdutf_sources)',
      ],
    },
  ]
}
