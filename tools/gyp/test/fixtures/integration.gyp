{
  'targets': [
    {
      'target_name': 'test',
      'type': 'executable',
      'sources': [
        'test.cc',
      ],
      'include_dirs': [
        'include',
      ],
      'library_dirs': [
        'mylib'
      ],
    },
  ]
}
