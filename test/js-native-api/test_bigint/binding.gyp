{
  "targets": [
    {
      "target_name": "test_bigint",
      "sources": [
        "test_bigint.c"
      ]
    },
    {
      "target_name": "test_bigint_vtable",
      "defines": [ "NODE_API_MODULE_USE_VTABLE" ],
      "sources": [
        "test_bigint.c"
      ]
    }
  ]
}
