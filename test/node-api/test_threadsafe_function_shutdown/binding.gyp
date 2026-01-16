{
  "targets": [
    {
      "target_name": "binding",
      "sources": ["binding.cc"],
      "cflags_cc": ["--std=c++20"],
      'cflags!': [ '-fno-exceptions', '-fno-rtti' ],
      'cflags_cc!': [ '-fno-exceptions', '-fno-rtti' ],
    },
    {
      "target_name": "binding_vtable",
      "defines": [ "NODE_API_MODULE_USE_VTABLE" ],
      "sources": ["binding.cc"],
      "cflags_cc": ["--std=c++20"],
      'cflags!': [ '-fno-exceptions', '-fno-rtti' ],
      'cflags_cc!': [ '-fno-exceptions', '-fno-rtti' ],
    }
  ]
}
