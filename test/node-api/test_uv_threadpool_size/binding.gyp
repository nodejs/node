{
  "targets": [
    {
      "target_name": "test_uv_threadpool_size",
      "sources": [ "test_uv_threadpool_size.c" ]
    },
    {
      "target_name": "test_uv_threadpool_size_vtable",
      "defines": [ "NODE_API_MODULE_USE_VTABLE" ],
      "sources": [ "test_uv_threadpool_size.c" ]
    }
  ]
}
