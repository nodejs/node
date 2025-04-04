{
  'variables': {
    'sqlite_sources': [
      'sqlite3.c',
    ],
  },
  'targets': [
    {
      'target_name': 'sqlite',
      'type': 'static_library',
      'cflags': ['-fvisibility=hidden'],
      'xcode_settings': {
        'GCC_SYMBOLS_PRIVATE_EXTERN': 'YES',  # -fvisibility=hidden
      },
      'defines': [
        'SQLITE_DEFAULT_MEMSTATUS=0',
        'SQLITE_ENABLE_COLUMN_METADATA',
        'SQLITE_ENABLE_DBSTAT_VTAB',
        'SQLITE_ENABLE_FTS3',
        'SQLITE_ENABLE_FTS3_PARENTHESIS',
        'SQLITE_ENABLE_FTS5',
        'SQLITE_ENABLE_GEOPOLY',
        'SQLITE_ENABLE_MATH_FUNCTIONS',
        'SQLITE_ENABLE_PREUPDATE_HOOK',
        'SQLITE_ENABLE_RBU',
        'SQLITE_ENABLE_RTREE',
        'SQLITE_ENABLE_SESSION',
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
}
