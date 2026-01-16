{
  "targets": [
    {
      "target_name": "test_fatal_exception",
      "sources": [ "test_fatal_exception.c" ]
    },
    {
      "target_name": "test_fatal_exception_vtable",
      "defines": [ "NODE_API_MODULE_USE_VTABLE" ],
      "sources": [ "test_fatal_exception.c" ]
    }
  ]
}
