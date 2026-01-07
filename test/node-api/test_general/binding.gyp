{
  "targets": [
    {
      "target_name": "test_general",
      "sources": [ "test_general.c" ]
    },
    {
      "target_name": "test_general_vtable",
      "defines": [ "NODE_API_MODULE_USE_VTABLE" ],
      "sources": [ "test_general.c" ]
    }
  ]
}
