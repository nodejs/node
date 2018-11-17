{
  'targets': [
    {
      'target_name': 'binding',
      'defines': [ 'V8_DEPRECATION_WARNINGS=1' ],
      'sources': [
        '../entry_point.c',
        'binding.c'
      ]
    }
  ]
}
