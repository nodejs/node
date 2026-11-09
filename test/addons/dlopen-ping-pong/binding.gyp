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
        # Deferred import of dlopen_pong (-Wl,-G) where ping.c links it directly.
        # AIX uses dlsym(RTLD_DEFAULT) in ping.c instead of a link-time import.
        ['OS=="os400"', {
          'ldflags': [ '-Wl,-G' ]
        }],
      ],
    },
    {
      'target_name': 'binding',
      'sources': [ 'binding.cc' ],
      'includes': ['../common.gypi'],
    }
  ]
}
