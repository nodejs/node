{
  'targets': [
    {
      'target_name': 'binding-export-default',
      'sources': [ 'binding-export-default.cc' ],
      'includes': ['../common.gypi'],
    },
    {
      'target_name': 'binding-export-primitive',
      'sources': [ 'binding-export-primitive.cc' ],
      'includes': ['../common.gypi'],
    },
  ]
}
