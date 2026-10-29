{
  "targets": [
    {
      "target_name": "test_uv_loop",
      "sources": [ "test_uv_loop.cc" ],
      "include_dirs": [ "<@(node_shared_libuv_include_dirs)" ],
    }
  ]
}
