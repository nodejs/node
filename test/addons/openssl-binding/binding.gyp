{
  'targets': [
    {
      'target_name': 'binding',
      'conditions': [
        ['node_use_openssl=="true"', {
          'sources': ['binding.cc'],
          'include_dirs': ['../../../deps/openssl/openssl/include'],
        }],
        [ 'OS in "linux freebsd openbsd solaris android aix cloudabi"', {
          'cflags': ['-Wno-cast-function-type'],
        }],
      ],
    },
  ]
}
