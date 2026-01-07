{
  'targets': [
    {
      'target_name': 'binding',
      'defines': [ 'V8_DEPRECATION_WARNINGS=1' ],
      'sources': [ 'binding.c' ]
    },
    {
      'target_name': 'binding_vtable',
      'defines': [
        'V8_DEPRECATION_WARNINGS=1',
        'NODE_API_MODULE_USE_VTABLE'
      ],
      'sources': [ 'binding.c' ]
    }
  ]
}
