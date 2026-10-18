{
  "targets": [
    {
      "target_name": "test_async",
      "sources": [ "test_async.c" ],
      "include_dirs": [ "<@(node_shared_libuv_include_dirs)" ],
    }
  ]
}
