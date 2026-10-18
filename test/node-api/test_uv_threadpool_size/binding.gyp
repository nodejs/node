{
  "targets": [
    {
      "target_name": "test_uv_threadpool_size",
      "sources": [ "test_uv_threadpool_size.c" ],
      "include_dirs": [ "<@(node_shared_libuv_include_dirs)" ],
    }
  ]
}
