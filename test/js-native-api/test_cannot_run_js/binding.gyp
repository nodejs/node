{
  "variables": {
    "common_sources": [
        "../entry_point.c",
    ]
  },
  "targets": [
    {
      "target_name": "test_cannot_run_js",
      "sources": [
        '<@(common_sources)',
        "test_cannot_run_js.c"
      ],
      "defines": [ "NAPI_EXPERIMENTAL" ],
    },
    {
      "target_name": "test_pending_exception",
      "sources": [
        '<@(common_sources)',
        "test_cannot_run_js.c"
      ],
      "defines": [ "NAPI_VERSION=8" ],
    }
  ]
}
