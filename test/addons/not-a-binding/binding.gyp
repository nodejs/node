{
  'targets': [
    {
      'target_name': 'binding',
      'sources': [ 'not_a_binding.c' ],
      'conditions': [
        [ 'OS in "linux freebsd openbsd solaris android aix cloudabi"', {
          'cflags': ['-Wno-cast-function-type'],
        }],
      ],
    }
  ]
}
