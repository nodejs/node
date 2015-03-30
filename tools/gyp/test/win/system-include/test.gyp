{
  'target_defaults': {
    'msvs_settings': {
      'VCCLCompilerTool': {
        'WarningLevel': '4',
        'WarnAsError': 'true',
      },
    },
    'msvs_system_include_dirs': [
      '$(ProjectName)',  # Different for each target
      'common',  # Same for all targets
    ],
  },
  'targets': [
    {
      'target_name': 'foo',
      'type': 'executable',
      'sources': [ 'main.cc', ],
    },
    {
      'target_name': 'bar',
      'type': 'executable',
      'sources': [ 'main.cc', ],
    },
  ],
}
