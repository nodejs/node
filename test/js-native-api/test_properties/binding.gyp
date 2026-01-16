{
  "targets": [
    {
      "target_name": "test_properties",
      "sources": [
        "test_properties.c"
      ]
    },
    {
      "target_name": "test_properties_vtable",
      "defines": [ "NODE_API_MODULE_USE_VTABLE" ],
      "sources": [
        "test_properties.c"
      ]
    }
  ]
}
