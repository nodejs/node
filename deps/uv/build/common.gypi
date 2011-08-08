{
  'conditions': [
    ['OS=="win"', {
      'target_defaults': {
        'msvs_settings': {
          'VCLinkerTool': {
            'GenerateDebugInformation': 'true',
          },
        },
      },
    }],  # OS=="win"
  ],
}