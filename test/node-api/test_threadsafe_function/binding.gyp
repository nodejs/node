{
  'targets': [
    {
      'target_name': 'binding',
      'sources': ['binding.c']
    },
    {
      'target_name': 'binding_vtable',
      'defines': [ 'NODE_API_MODULE_USE_VTABLE' ],
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
      'target_name': 'test_uncaught_exception_v9_vtable',
      'defines': [
        'NAPI_VERSION=9',
        'NODE_API_MODULE_USE_VTABLE'
      ],
      'sources': ['test_uncaught_exception.c']
    },
    {
      'target_name': 'test_uncaught_exception',
      'defines': [
        'NAPI_EXPERIMENTAL'
      ],
      'sources': ['test_uncaught_exception.c']
    },
    {
      'target_name': 'test_uncaught_exception_vtable',
      'defines': [
        'NAPI_EXPERIMENTAL',
        'NODE_API_MODULE_USE_VTABLE'
      ],
      'sources': ['test_uncaught_exception.c']
    }
  ]
}
