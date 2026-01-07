{
  "targets": [
    {
      "target_name": "test_conversions",
      "sources": [
        "test_conversions.c",
        "test_null.c",
      ]
    },
    {
      "target_name": "test_conversions_vtable",
      "defines": [ "NODE_API_MODULE_USE_VTABLE" ],
      "sources": [
        "test_conversions.c",
        "test_null.c",
      ]
    }
  ]
}
