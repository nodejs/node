{
  "targets": [
    {
      "target_name": "myobject",
      "sources": [
        "myobject.cc",
        "myobject.h",
      ]
    },
    {
      "target_name": "myobject_vtable",
      "defines": [ "NODE_API_MODULE_USE_VTABLE" ],
      "sources": [
        "myobject.cc",
        "myobject.h",
      ]
    },
    {
      "target_name": "myobject_basic_finalizer",
      "defines": [ "NAPI_EXPERIMENTAL" ],
      "sources": [
        "myobject.cc",
        "myobject.h",
      ]
    },
    {
      "target_name": "myobject_basic_finalizer_vtable",
      "defines": [ "NAPI_EXPERIMENTAL", "NODE_API_MODULE_USE_VTABLE" ],
      "sources": [
        "myobject.cc",
        "myobject.h",
      ]
    },
    {
      "target_name": "nested_wrap",
      # Test without basic finalizers as it schedules differently.
      "defines": [ "NAPI_VERSION=10" ],
      "sources": [
        "nested_wrap.cc",
        "nested_wrap.h",
      ],
    },
    {
      "target_name": "nested_wrap_vtable",
      # Test without basic finalizers as it schedules differently.
      "defines": [ "NAPI_VERSION=10", "NODE_API_MODULE_USE_VTABLE" ],
      "sources": [
        "nested_wrap.cc",
        "nested_wrap.h",
      ]
    }
  ]
}
