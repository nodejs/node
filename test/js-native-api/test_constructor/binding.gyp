{
  "targets": [
    {
      "target_name": "test_constructor",
      "sources": [
        "test_constructor.c",
        "test_null.c",
      ]
    },
    {
      "target_name": "test_constructor_vtable",
      "defines": [ "NODE_API_MODULE_USE_VTABLE" ],
      "sources": [
        "test_constructor.c",
        "test_null.c",
      ]
    }
  ]
}
