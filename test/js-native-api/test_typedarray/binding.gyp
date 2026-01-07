{
  "targets": [
    {
      "target_name": "test_typedarray",
      "sources": [
        "test_typedarray.c"
      ]
    },
    {
      "target_name": "test_typedarray_vtable",
      "defines": [ "NODE_API_MODULE_USE_VTABLE" ],
      "sources": [
        "test_typedarray.c"
      ]
    }
  ]
}
