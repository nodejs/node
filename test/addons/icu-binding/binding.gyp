{
  'targets': [
    {
      'target_name': 'binding',
      'defines': [ 'V8_DEPRECATION_WARNINGS=1' ],
      'conditions': [
         ['v8_enable_i18n_support==1', {
           'sources': ['binding.cc'],
           'include_dirs': [
             '../../../deps/icu-small/source/common',
             '../../../deps/icu-small/source/i18n',
           ],
         }]
      ]
    },
  ]
}
