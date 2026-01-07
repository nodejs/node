{
  "targets": [
    {
      "target_name": "test_array",
      "sources": [
        "test_array.c"
      ]
    },
    {
      "target_name": "test_array_vtable",
      "defines": [ "NODE_API_MODULE_USE_VTABLE" ],
      "sources": [
        "test_array.c"
      ]
    }
  ]
}
