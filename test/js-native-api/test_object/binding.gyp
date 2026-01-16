{
  "targets": [
    {
      "target_name": "test_object",
      "sources": [
        "test_null.c",
        "test_object.c"
      ],
      "defines": [
        "NAPI_EXPERIMENTAL"
      ],
    },
    {
      "target_name": "test_object_vtable",
      "defines": [ "NAPI_EXPERIMENTAL", "NODE_API_MODULE_USE_VTABLE" ],
      "sources": [
        "test_null.c",
        "test_object.c"
      ],
    },
    {
      "target_name": "test_exceptions",
      "sources": [
        "test_exceptions.c",
      ]
    },
    {
      "target_name": "test_exceptions_vtable",
      "defines": [ "NODE_API_MODULE_USE_VTABLE" ],
      "sources": [
        "test_exceptions.c",
      ]
    }
  ]
}
