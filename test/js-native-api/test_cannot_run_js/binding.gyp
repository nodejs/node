{
  "targets": [
    {
      "target_name": "test_cannot_run_js",
      "defines": [ "NAPI_VERSION=10" ],
      "sources": [
        "test_cannot_run_js.c"
      ],
    },
    {
      "target_name": "test_cannot_run_js_vtable",
      "defines": [ "NAPI_VERSION=10", "NODE_API_MODULE_USE_VTABLE" ],
      "sources": [
        "test_cannot_run_js.c"
      ],
    },
    {
      "target_name": "test_pending_exception",
      "defines": [ "NAPI_VERSION=9" ],
      "sources": [
        "test_cannot_run_js.c"
      ],
    },
    {
      "target_name": "test_pending_exception_vtable",
      "defines": [ "NAPI_VERSION=9", "NODE_API_MODULE_USE_VTABLE" ],
      "sources": [
        "test_cannot_run_js.c"
      ],
    }
  ]
}
