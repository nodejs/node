{
  "targets": [
    {
      "target_name": "test_fatal",
      "sources": [ "test_fatal.c" ]
    },
    {
      "target_name": "test_fatal_vtable",
      "defines": [ "NODE_API_MODULE_USE_VTABLE" ],
      "sources": [ "test_fatal.c" ]
    }
  ]
}
