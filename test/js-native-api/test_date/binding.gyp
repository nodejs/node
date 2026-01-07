{
  "targets": [
    {
      "target_name": "test_date",
      "sources": [
        "test_date.c"
      ]
    },
    {
      "target_name": "test_date_vtable",
      "defines": [ "NODE_API_MODULE_USE_VTABLE" ],
      "sources": [
        "test_date.c"
      ]
    }
  ]
}
