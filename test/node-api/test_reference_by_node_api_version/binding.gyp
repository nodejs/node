{
  "targets": [
    {
      "target_name": "test_reference_all_types",
      "sources": [ "test_reference_by_node_api_version.c" ],
      "defines": [ "NAPI_VERSION=10" ],
    },
    {
      "target_name": "test_reference_all_types_vtable",
      "sources": [ "test_reference_by_node_api_version.c" ],
      "defines": [
        "NAPI_VERSION=10",
        "NODE_API_MODULE_USE_VTABLE",
      ],
    },
    {
      "target_name": "test_reference_obj_only",
      "sources": [ "test_reference_by_node_api_version.c" ],
      "defines": [ "NAPI_VERSION=9" ],
    },
    {
      "target_name": "test_reference_obj_only_vtable",
      "sources": [ "test_reference_by_node_api_version.c" ],
      "defines": [
        "NAPI_VERSION=9",
        "NODE_API_MODULE_USE_VTABLE",
      ],
    }
  ]
}
