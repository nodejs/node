{
  'variables': {
    'openssl_no_asm%': 0,
  },
  'targets': [
    {
      'target_name': 'openssl',
      'type': '<(library)',
      'includes': ['./openssl_common.gypi'],
      'defines': [
        # Compile out hardware engines.  Most are stubs that dynamically load
        # the real driver but that poses a security liability when an attacker
        # is able to create a malicious DLL in one of the default search paths.
        'OPENSSL_NO_HW',
      ],
      'conditions': [
        [ 'openssl_no_asm==0', {
          'includes': ['./openssl_asm.gypi'],
        }, {
          'includes': ['./openssl_no_asm.gypi'],
        }],
      ],
      'direct_dependent_settings': {
        'include_dirs': [ 'openssl/include']
      }
    }, {
      # openssl-cli target
      'target_name': 'openssl-cli',
      'type': 'executable',
      'dependencies': ['openssl'],
      'includes': ['./openssl_common.gypi'],
      'conditions': [
        ['openssl_no_asm==0', {
          'includes': ['./openssl-cl_asm.gypi'],
        }, {
          'includes': ['./openssl-cl_no_asm.gypi'],
        }],
      ],
    },
  ],
}
