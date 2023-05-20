{
  "targets": [
    {
      "target_name": "copy_entry_point",
      "type": "none",
      "copies": [
        {
          "destination": ".",
          "files": [ "../entry_point.c" ]
        }
      ]
    },
    {
      "target_name": "test_cannot_run_js",
      "sources": [
        "entry_point.c",
        "test_cannot_run_js.c"
      ],
      "defines": [ "NAPI_EXPERIMENTAL" ],
      "dependencies": [ "copy_entry_point" ],
    },
    {
      "target_name": "test_pending_exception",
      "sources": [
        "entry_point.c",
        "test_cannot_run_js.c"
      ],
      "defines": [ "NAPI_VERSION=8" ],
      "dependencies": [ "copy_entry_point" ],
    }
  ]
}
