{
  "targets": [
    {
      "target_name": "test_buffer",
      "defines": [
        'NAPI_VERSION=10'
      ],
      "sources": [ "test_buffer.c" ]
    },
    {
      "target_name": "test_finalizer",
      "sources": [ "test_finalizer.c" ]
    }
  ]
}
