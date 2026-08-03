{
  "targets": [
    {
      "target_name": "test_object",
      "sources": [
        "test_null.c",
        "test_object.c"
      ],
      "defines": [
        "NAPI_EXPERIMENTAL"
      ],
    },
    {
      "target_name": "test_exceptions",
      "sources": [
        "test_exceptions.c",
      ]
    }
  ]
}
