{
  "targets": [
    {
      "target_name": "test_reference_double_free",
      "sources": [
        "test_reference_double_free.c"
      ]
    },
    {
      "target_name": "test_reference_double_free_vtable",
      "defines": [ "NODE_API_MODULE_USE_VTABLE" ],
      "sources": [
        "test_reference_double_free.c"
      ]
    }
  ]
}
