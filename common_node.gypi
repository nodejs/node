{
  'target_defaults': {
    'conditions': [
      # Override common.gypi config to use C++20 for Node.js core only.
      ['OS in "linux freebsd openbsd solaris android aix os400 cloudabi"', {
        'cflags_cc!': ['-std=gnu++17'],
        'cflags_cc': ['-std=gnu++20'],
      }],
      ['OS=="mac" and clang==1', {
        'xcode_settings': {
          'CLANG_CXX_LANGUAGE_STANDARD': 'gnu++20',  # -std=gnu++20
        },
      }],
    ],
  },
}
