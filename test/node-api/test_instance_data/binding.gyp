{
  "targets": [
    {
      "target_name": "test_instance_data",
      "sources": [
        "test_instance_data.c"
      ],
      "include_dirs": [ "<@(node_shared_libuv_include_dirs)" ],
    },
    {
      "target_name": "test_set_then_ref",
      "sources": [
        "addon.c",
        "test_set_then_ref.c",
      ]
    },
    {
      "target_name": "test_ref_then_set",
      "sources": [
        "addon.c",
        "test_ref_then_set.c",
      ]
    },
  ]
}
