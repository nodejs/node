{
  "targets": [
    {
      "target_name": "test_error",
      "sources": [
        "test_error.c"
      ]
    },
    {
      "target_name": "test_error_vtable",
      "defines": [ "NODE_API_MODULE_USE_VTABLE" ],
      "sources": [
        "test_error.c"
      ]
    }
  ]
}
