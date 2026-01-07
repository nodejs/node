{
  "targets": [
    {
      "target_name": "test_reference",
      "sources": [
        "test_reference.c"
      ]
    },
    {
      "target_name": "test_reference_vtable",
      "defines": [ "NODE_API_MODULE_USE_VTABLE" ],
      "sources": [
        "test_reference.c"
      ]
    },
    {
      "target_name": "test_finalizer",
      "sources": [
        "test_finalizer.c"
      ]
    },
    {
      "target_name": "test_finalizer_vtable",
      "defines": [ "NODE_API_MODULE_USE_VTABLE" ],
      "sources": [
        "test_finalizer.c"
      ]
    }
  ]
}
