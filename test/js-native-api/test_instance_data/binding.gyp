{
  "targets": [
    {
      "target_name": "test_instance_data",
      "sources": [
        "test_instance_data.c"
      ]
    },
    {
      "target_name": "test_instance_data_vtable",
      "defines": [ "NODE_API_MODULE_USE_VTABLE" ],
      "sources": [
        "test_instance_data.c"
      ]
    }
  ]
}
