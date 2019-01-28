{
  'targets': [
    {
      'target_name': 'binding',
      'sources': [ 'binding.cc' ],
      'conditions': [
        [ 'OS in "linux freebsd openbsd solaris android aix cloudabi"', {
          'cflags': ['-Wno-cast-function-type'],
        }],
      ],
    }
  ]
}
