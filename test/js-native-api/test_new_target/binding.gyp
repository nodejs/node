{
  'targets': [
    {
      'target_name': 'test_new_target',
      'defines': [ 'V8_DEPRECATION_WARNINGS=1' ],
      'sources': [
        'test_new_target.c'
      ]
    },
    {
      'target_name': 'test_new_target_vtable',
      'defines': [ 'V8_DEPRECATION_WARNINGS=1', 'NODE_API_MODULE_USE_VTABLE' ],
      'sources': [
        'test_new_target.c'
      ]
    }
  ]
}
