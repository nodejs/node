{
  'targets': [
    {
      'target_name': 'binding',
      'defines': [
        'NODE_WANT_INTERNALS=1',
        'V8_DEPRECATION_WARNINGS=1',
      ],
      'sources': [ 'binding.cc' ]
    }
  ]
}
