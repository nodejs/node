{
  'targets': [
    {
      'target_name': 'test_new_target',
      'defines': [ 'V8_DEPRECATION_WARNINGS=1' ],
      'sources': [
        '../entry_point.c',
        'test_new_target.c'
      ]
    }
  ]
}
