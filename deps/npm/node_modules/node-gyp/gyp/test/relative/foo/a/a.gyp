{
  'targets': [
    {
      'target_name': 'a',
      'type': 'executable',
      'sources': ['a.cc'],
      'dependencies': [
        '../../foo/b/b.gyp:b',
        'c/c.gyp:c',
      ],
    },
  ],
}
