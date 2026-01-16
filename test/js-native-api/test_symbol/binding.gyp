{
  "targets": [
    {
      "target_name": "test_symbol",
      "sources": [
        "test_symbol.c"
      ]
    },
    {
      "target_name": "test_symbol_vtable",
      "defines": [ "NODE_API_MODULE_USE_VTABLE" ],
      "sources": [
        "test_symbol.c"
      ]
    }
  ]
}
