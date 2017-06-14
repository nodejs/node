{
  'targets': [
    {
      'target_name': 'ping',
      'product_extension': 'so',
      'type': 'shared_library',
      'sources': [ 'ping.c' ],
      'conditions': [
        ['OS=="mac"', {
          'xcode_settings': {
            'OTHER_LDFLAGS': [ '-Wl,-undefined', '-Wl,dynamic_lookup' ]
        }}],
        # Pass erok flag to the linker, to prevent unresolved symbols
        # from failing. Still, the test won't pass, so we'll skip it on AIX.
        ['OS=="aix"', {
          'ldflags': [ '-Wl,-berok' ]
        }]],
    },
    {
      'target_name': 'binding',
      'defines': [ 'V8_DEPRECATION_WARNINGS=1' ],
      'sources': [ 'binding.cc' ],
    }
  ]
}
