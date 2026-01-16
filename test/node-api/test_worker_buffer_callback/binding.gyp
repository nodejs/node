{
  'targets': [
    {
      'target_name': 'binding',
      'sources': [ 'test_worker_buffer_callback.c' ]
    },
    {
      'target_name': 'binding_vtable',
      'defines': [ 'NODE_API_MODULE_USE_VTABLE' ],
      'sources': [ 'test_worker_buffer_callback.c' ]
    }
  ]
}
