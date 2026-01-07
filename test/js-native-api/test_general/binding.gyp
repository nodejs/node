{
  "targets": [
    {
      "target_name": "test_general",
      "defines": [ "NAPI_EXPERIMENTAL" ],
      "sources": [
        "test_general.c"
      ],
    },
    {
      "target_name": "test_general_vtable",
      "defines": [ "NAPI_EXPERIMENTAL", "NODE_API_MODULE_USE_VTABLE" ],
      "sources": [
        "test_general.c"
      ]
    }
  ]
}
