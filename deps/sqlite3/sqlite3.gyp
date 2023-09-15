{
  'variables': {
    'sqlite3_sources': [
      'sqlite3.c',
    ],
  },
  'targets': [
    {
      'target_name': 'sqlite3',
      'type': 'static_library',
      'include_dirs': ['.'],
      'sources': [
        '<@(sqlite3_sources)',
      ],
      'direct_dependent_settings': {
        'include_dirs': ['.'],
      },
    },
  ],
}
