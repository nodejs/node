{
  "targets": [
    {
      "target_name": "test_handle_scope",
      "sources": [
        "test_handle_scope.c"
      ]
    },
    {
      "target_name": "test_handle_scope_vtable",
      "defines": [ "NODE_API_MODULE_USE_VTABLE" ],
      "sources": [
        "test_handle_scope.c"
      ]
    }
  ]
}
