{
  "targets": [
    {
      "target_name": "test_uv_loop",
      "sources": [ "test_uv_loop.cc" ]
    },
    {
      "target_name": "test_uv_loop_vtable",
      "defines": [ "NODE_API_MODULE_USE_VTABLE" ],
      "sources": [ "test_uv_loop.cc" ]
    }
  ]
}
