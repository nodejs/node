{
  "targets": [
    {
      "target_name": "test_dataview",
      # NAPI_EXPERIMENTAL for node_api_is_sharedarraybuffer
      "defines": [ "NAPI_EXPERIMENTAL", "NODE_API_EXPERIMENTAL_NO_WARNING" ],
      "sources": [
        "test_dataview.c"
      ],
    },
    {
      "target_name": "test_dataview_vtable",
      "defines": [ "NAPI_EXPERIMENTAL", "NODE_API_EXPERIMENTAL_NO_WARNING", "NODE_API_MODULE_USE_VTABLE" ],
      "sources": [
        "test_dataview.c"
      ],
    }
  ]
}
