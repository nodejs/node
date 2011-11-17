{
  'includes': [
    '../../build/common.gypi',
    'v8.gyp'
  ],
  'target_defaults': {
    'conditions': [
      [ 'OS=="solaris"', {
        'defines': [ '__C99FEATURES__=1' ],
      }],
    ],
  },
}
