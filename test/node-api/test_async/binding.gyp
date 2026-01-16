{
  "targets": [
    {
      "target_name": "test_async",
      "sources": [ "test_async.c" ]
    },
    {
      "target_name": "test_async_vtable",
      "defines": [ "NODE_API_MODULE_USE_VTABLE" ],
      "sources": [ "test_async.c" ]
    }
  ]
}
