{
  'targets': [
    {
      'target_name': 'binding',
      'defines': [ 'V8_DEPRECATION_WARNINGS=1' ],
      'sources': [ 'binding.cc' ],
      'conditions': [
        [ 'OS in "linux freebsd openbsd solaris android aix cloudabi"', {
          'cflags': ['-Wno-cast-function-type'],
        }],
      ],
    }
  ]
}
