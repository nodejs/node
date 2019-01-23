{
  'targets': [
    {
      'target_name': 'binding',
      'sources': [ 'not_a_binding.c' ],
      'cflags': ['-Wno-cast-function-type'],
    }
  ]
}
