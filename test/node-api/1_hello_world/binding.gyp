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
    },
    {
      "target_name": "binding_vtable_nofb",
      "defines": [ "NODE_API_MODULE_USE_VTABLE", "NODE_API_MODULE_NO_VTABLE_FALLBACK" ],
      "sources": [ "binding.c" ]
    }
  ]
}
