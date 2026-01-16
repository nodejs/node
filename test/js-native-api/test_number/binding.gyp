{
  "targets": [
    {
      "target_name": "test_number",
      "sources": [
        "test_number.c",
        "test_null.c",
      ]
    },
    {
      "target_name": "test_number_vtable",
      "defines": [ "NODE_API_MODULE_USE_VTABLE" ],
      "sources": [
        "test_number.c",
        "test_null.c",
      ]
    }
  ]
}
