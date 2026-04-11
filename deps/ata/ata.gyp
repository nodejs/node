{
  'variables': {
    'ata_sources': [
      'ata.cpp',
    ]
  },
  'targets': [
    {
      'target_name': 'ata',
      'type': 'static_library',
      'include_dirs': ['.'],
      'direct_dependent_settings': {
        'include_dirs': ['.'],
      },
      'conditions': [
        ['node_shared_simdjson=="false"', {
          'dependencies': [
            '../simdjson/simdjson.gyp:simdjson',
          ],
        }],
        ['OS=="win"', {
          'msvs_settings': {
            'VCCLCompilerTool': {
              'AdditionalOptions': ['/std:c++20'],
            },
          },
        }, {
          'cflags_cc': ['-std=c++20'],
          'xcode_settings': {
            'CLANG_CXX_LANGUAGE_STANDARD': 'c++20',
          },
        }],
      ],
      'defines': [
        'ATA_NO_RE2',
      ],
      'sources': [
        '<@(ata_sources)',
      ],
    },
  ]
}
