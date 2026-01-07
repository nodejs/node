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
    },
    {
      "target_name": "test_set_then_ref",
      "sources": [
        "addon.c",
        "test_set_then_ref.c",
      ]
    },
    {
      "target_name": "test_set_then_ref_vtable",
      "defines": [ "NODE_API_MODULE_USE_VTABLE" ],
      "sources": [
        "addon.c",
        "test_set_then_ref.c",
      ]
    },
    {
      "target_name": "test_ref_then_set",
      "sources": [
        "addon.c",
        "test_ref_then_set.c",
      ]
    },
    {
      "target_name": "test_ref_then_set_vtable",
      "defines": [ "NODE_API_MODULE_USE_VTABLE" ],
      "sources": [
        "addon.c",
        "test_ref_then_set.c",
      ]
    },
  ]
}
