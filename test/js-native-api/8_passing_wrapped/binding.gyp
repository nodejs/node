{
  "targets": [
    {
      "target_name": "8_passing_wrapped",
      "sources": [
        "8_passing_wrapped.cc",
        "myobject.cc"
      ]
    },
    {
      "target_name": "8_passing_wrapped_vtable",
      "defines": [ "NODE_API_MODULE_USE_VTABLE" ],
      "sources": [
        "8_passing_wrapped.cc",
        "myobject.cc"
      ]
    }
  ]
}
