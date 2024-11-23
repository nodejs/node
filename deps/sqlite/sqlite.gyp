{
  'variables': {
    'sqlite_sources': [
      'sqlite3.c',
    ],
    'use_system_sqlite%': 0,
  },
  'conditions': [
    ['use_system_sqlite==0', {
      'targets': [
        {
          'target_name': 'sqlite',
          'type': 'static_library',
          'cflags': ['-fvisibility=hidden'],
          'xcode_settings': {
            'GCC_SYMBOLS_PRIVATE_EXTERN': 'YES',  # -fvisibility=hidden
          },
          'defines': [
            'SQLITE_ENABLE_SESSION',
            'SQLITE_ENABLE_PREUPDATE_HOOK'
          ],
          'include_dirs': ['.'],
          'sources': [
            '<@(sqlite_sources)',
          ],
          'direct_dependent_settings': {
            'include_dirs': ['.'],
          },
        },
      ],
    }, {
      'targets': [
        {
          'target_name': 'sqlite',
          'type': 'static_library',
          'direct_dependent_settings': {
            'defines': [
              'USE_SYSTEM_SQLITE',
            ],
          },
          'defines': [
            'USE_SYSTEM_SQLITE',
          ],
          'link_settings': {
            'libraries': [
              '-lsqlite',
            ],
          },
        },
      ],
    }],
  ],
}
