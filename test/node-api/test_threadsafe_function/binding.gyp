{
  'targets': [
    {
      'target_name': 'binding',
      'sources': ['binding.c']
    },
    {
      'target_name': 'test_uncaught_exception_v9',
      'defines': [
        'NAPI_VERSION=9'
      ],
      'sources': ['test_uncaught_exception.c']
    },
    {
      'target_name': 'test_uncaught_exception',
      'defines': [
        'NAPI_EXPERIMENTAL'
      ],
      'sources': ['test_uncaught_exception.c']
    }
  ]
}
