{
  'targets': [
    {
      'target_name': 'binding',
      'sources': [ 'binding.cc' ],
      'defines': [ 'NAPI_EXPERIMENTAL' ],
      'cflags': ['-v'],
      'variables': {
        'CXX': '<!(echo $CXX)'
      }
    },
    {
      'target_name': 'binding2',
      'sources': [ 'binding2.cc' ],
      'defines': [ 'NAPI_EXPERIMENTAL', 'NODE_API_EXPERIMENTAL_NO_WARNING' ],
      'cflags': ['-v'],
      'variables': {
        'CXX': '<!(echo $CXX)'
      }
    }
  ]
}
