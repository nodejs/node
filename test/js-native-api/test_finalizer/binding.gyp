{
  "targets": [
    {
      "target_name": "test_finalizer",
      "defines": [ "NAPI_EXPERIMENTAL" ],
      "sources": [
        "test_finalizer.c"
      ]
    },
    {
      "target_name": "test_finalizer_vtable",
      "defines": [ "NAPI_EXPERIMENTAL", "NODE_API_MODULE_USE_VTABLE" ],
      "sources": [
        "test_finalizer.c"
      ]
    }
  ]
}
