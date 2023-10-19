{
  "targets": [
    {
      "target_name": "test_cannot_run_js",
      "sources": [
        "test_cannot_run_js.c"
      ],
      "defines": [ "NAPI_EXPERIMENTAL" ],
    },
    {
      "target_name": "test_pending_exception",
      "sources": [
        "test_cannot_run_js.c"
      ],
      "defines": [ "NAPI_VERSION=8" ],
    }
  ]
}
