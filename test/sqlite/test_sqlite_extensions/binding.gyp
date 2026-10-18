{
  "targets": [
    {
      "target_name": "sqlite_extension",
      "type": "shared_library",
      "sources": [ "extension.c" ],
      "conditions": [
        ['node_shared_sqlite=="false"', {
          "include_dirs": [ "../../../deps/sqlite" ],
        }, {
          "include_dirs": [ "<@(node_shared_sqlite_include_dirs)" ],
        }],
      ],
    }
  ]
}
