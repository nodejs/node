{
  "targets": [
    {
      "target_name": "test_buffer",
      "defines": [
        'NAPI_VERSION=10'
      ],
      "sources": [ "test_buffer.c" ]
    },
    {
      "target_name": "test_buffer_vtable",
      "defines": [
        'NAPI_VERSION=10',
        'NODE_API_MODULE_USE_VTABLE'
      ],
      "sources": [ "test_buffer.c" ]
    },
    {
      "target_name": "test_finalizer",
      "sources": [ "test_finalizer.c" ]
    },
    {
      "target_name": "test_finalizer_vtable",
      "defines": [ 'NODE_API_MODULE_USE_VTABLE' ],
      "sources": [ "test_finalizer.c" ]
    }
  ]
}
