{
  'targets': [
    {
      'target_name': 'lib',
      'type': 'static_library',
      'sources': ['lib.cc'],
      'arflags': ['--nonexistent'],
    },
  ],
}
