{
  "targets": [
    {
      "target_name": "test_promise",
      "sources": [
        "test_promise.c"
      ]
    },
    {
      "target_name": "test_promise_vtable",
      "defines": [ "NODE_API_MODULE_USE_VTABLE" ],
      "sources": [
        "test_promise.c"
      ]
    }
  ]
}
