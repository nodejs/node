{
  "targets": [
    {
      "target_name": "test_string",
      "defines": [ "NAPI_VERSION=10" ],
      "sources": [
        "test_string.c",
        "test_null.c",
      ],
    },
    {
      "target_name": "test_string_vtable",
      "defines": [ "NAPI_VERSION=10", "NODE_API_MODULE_USE_VTABLE" ],
      "sources": [
        "test_string.c",
        "test_null.c",
      ],
    },
  ],
}
