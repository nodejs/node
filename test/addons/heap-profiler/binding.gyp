{
  'targets': [
    {
      'target_name': 'binding',
      'defines': [ 'V8_DEPRECATION_WARNINGS=1' ],
      'sources': [ 'binding.cc' ],
      'win_delay_load_hook': 'false'
    }
  ]
}
