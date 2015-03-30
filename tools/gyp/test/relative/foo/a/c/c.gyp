{
  'targets': [
    {
      'target_name': 'c',
      'type': 'static_library',
      'sources': ['c.cc'],
      'dependencies': [
        '../../b/b.gyp:b',
      ],
    },
  ],
}
