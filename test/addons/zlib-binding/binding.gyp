{
  'targets': [
    {
      'target_name': 'binding',
      'sources': ['binding.cc'],
      'include_dirs': ['../../../deps/zlib'],
      'cflags': ['-Wno-cast-function-type'],
    },
  ]
}
