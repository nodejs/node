{
  "targets": [
    {
      "target_name": "test_dataview",
      "sources": [
        "test_dataview.c"
      ],

      # For node_api_is_sharedarraybuffer
      'defines': [ 'NAPI_EXPERIMENTAL', 'NODE_API_EXPERIMENTAL_NO_WARNING' ]
    }
  ]
}
