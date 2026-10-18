{
  "targets": [
    {
      "target_name": "test_fatal",
      "sources": [ "test_fatal.c" ],
      "include_dirs": [ "<@(node_shared_libuv_include_dirs)" ],
    }
  ]
}
