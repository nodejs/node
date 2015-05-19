{
  'targets': [
    {
      'target_name': 'getopt',
      'type': 'static_library',
      'sources': [
        'getopt.h',
        'getopt_long.c'
      ],
      'defines': [
        'REPLACE_GETOPT'
      ]
    }
  ]
}
