{
  "targets": [
    {
      "target_name": "7_factory_wrap",
      "sources": [
        "7_factory_wrap.cc",
        "myobject.cc"
      ]
    },
    {
      "target_name": "7_factory_wrap_vtable",
      "defines": [ "NODE_API_MODULE_USE_VTABLE" ],
      "sources": [
        "7_factory_wrap.cc",
        "myobject.cc"
      ]
    }
  ]
}
