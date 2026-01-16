{
  "targets": [
    {
      "target_name": "test_sharedarraybuffer",
      "sources": [ "test_sharedarraybuffer.c" ]
    },
    {
      "target_name": "test_sharedarraybuffer_vtable",
      "defines": [ "NODE_API_MODULE_USE_VTABLE" ],
      "sources": [ "test_sharedarraybuffer.c" ]
    }
  ]
}
