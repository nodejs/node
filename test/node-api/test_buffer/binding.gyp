{
  "targets": [
    {
      "target_name": "test_buffer",
      "defines": [
        "NAPI_EXPERIMENTAL",
        "NAPI_VERSION=10",
        "NODE_API_EXPERIMENTAL_NO_WARNING"
      ],
      "sources": [ "test_buffer.c" ]
    },
    {
      "target_name": "test_finalizer",
      "sources": [ "test_finalizer.c" ]
    }
  ]
}
