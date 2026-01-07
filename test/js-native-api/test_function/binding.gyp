{
  "targets": [
    {
      "target_name": "test_function",
      "sources": [
        "test_function.c"
      ]
    },
    {
      "target_name": "test_function_vtable",
      "defines": [ "NODE_API_MODULE_USE_VTABLE" ],
      "sources": [
        "test_function.c"
      ]
    }
  ]
}
