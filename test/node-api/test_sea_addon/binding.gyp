{
  "targets": [
    {
      "target_name": "binding",
      "sources": [ "binding.c" ]
    },
    {
      "target_name": "binding_vtable",
      "defines": [ "NODE_API_MODULE_USE_VTABLE" ],
      "sources": [ "binding.c" ]
    }
  ]
}
