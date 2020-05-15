{
  'targets': [
    {
      'target_name': 'cjs-module-lexer',
      'type': 'static_library',
      'cflags': ['-Wno-implicit-fallthrough', '-Wno-parentheses'],
      'include_dirs': ['include'],
      'sources': [
        'src/lexer.c',
      ],
      'direct_dependent_settings': {
        'include_dirs': ['include']
      },
    }
  ]
}