{
  'includes': [ '../../icu_config.gypi' ],
  'targets': [
    {
      'target_name': 'noderes',
      'type': '<(library)',
      'conditions': [
        [
          'v8_enable_i18n_support==1',
          {
            'include_dirs': [ 'include', 'src' ],
            'direct_dependent_settings': {
              'include_dirs': [ 'include' ]
            },
            'sources': [
              'include/l10n.h',
              'include/l10n_version.h',
              'src/l10n.cc'
            ],
            'dependencies': [
              '<(icu_gyp_path):icui18n',
              '<(icu_gyp_path):icuuc',
              'icu_noderes_data'
            ]
          }
        ]
      ]
    },
    {
      #### build the resource bundle using ICU's tools ####
      'target_name': 'icu_noderes_data',
      'type': '<(library)',
      'conditions': [
        [
          'v8_enable_i18n_support==1',
          {
            'toolsets': [ 'target' ],
            'dependencies': [
              '<(icu_gyp_path):icu_implementation#host',
              '<(icu_gyp_path):icu_uconfig',
              '<(icu_gyp_path):genrb#host',
              '<(icu_gyp_path):genccode#host',
              '<(icu_gyp_path):icupkg#host'
            ],
            'actions': [
              {
                'action_name': 'icures',
                'inputs': [
                  'resources/root.txt',
                  'resources/en.txt',
                  'resources/en_US.txt'
                ],
                'outputs': [
                  '<(SHARED_INTERMEDIATE_DIR)/noderestmp/node.dat'
                ],
                'action': [
                  'python',
                  'icures.py',
                  '-n', 'node',
                  '-d', '<(SHARED_INTERMEDIATE_DIR)/noderestmp',
                  '-i', '<(PRODUCT_DIR)'
                ]
              },
              {
                'action_name': 'icugen',
                'inputs': [
                  '<(SHARED_INTERMEDIATE_DIR)/noderestmp/node.dat'
                ],
                'conditions': [
                  [
                    'OS != "win"',
                    {
                      'outputs': [
                        '<(SHARED_INTERMEDIATE_DIR)/node_dat.c'
                      ],
                      'action': [
                        '<(PRODUCT_DIR)/genccode',
                        '-e', 'node',
                        '-d', '<(SHARED_INTERMEDIATE_DIR)',
                        '-f', 'node_dat',
                        '<@(_inputs)'
                      ]
                    },
                    {
                      'outputs': [
                        '<(SHARED_INTERMEDIATE_DIR)/node_dat.obj'
                      ],
                      'action': [
                        '<(PRODUCT_DIR)/genccode',
                        '-o',
                        '-d', '<(SHARED_INTERMEDIATE_DIR)',
                        '-n', 'node',
                        '-e', 'node',
                        '<@(_inputs)'
                      ]
                    }
                  ]
                ]
              }
            ],
            'conditions': [
              [ 'OS == "win"',
                {
                  'sources': [
                    '<(SHARED_INTERMEDIATE_DIR)/node_dat.obj'
                  ]
                },
                {
                  'include_dirs': [
                    '../icu/source/common'
                  ],
                  'sources': [
                    '<(SHARED_INTERMEDIATE_DIR)/node_dat.c'
                  ]
                }
              ]
            ]
          }
        ]
      ]
    }
  ]
}
