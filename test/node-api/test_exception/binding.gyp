{
  "targets": [
    {
      "target_name": "test_exception",
      "sources": [ "test_exception.c" ]
    },
    {
      "target_name": "test_exception_vtable",
      "defines": [ "NODE_API_MODULE_USE_VTABLE" ],
      "sources": [ "test_exception.c" ]
    }
  ]
}
