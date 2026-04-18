{
  'variables': {
    'cargo%': 'cargo',
    'cargo_vendor_dir': './vendor',
    'cargo_rust_target%': '',
  },
  'conditions': [
    ['build_type == "Release"', {
      'variables': {
        'cargo_build_flags': ['--release'],
      },
      'conditions': [
        ['cargo_rust_target!=""', {
          'variables': {
            'node_crates_libpath': '<(SHARED_INTERMEDIATE_DIR)/$(Platform)/release/node_crates.lib',
          },
        }, {
          'variables': {
            'node_crates_libpath': '<(SHARED_INTERMEDIATE_DIR)/release/<(STATIC_LIB_PREFIX)node_crates<(STATIC_LIB_SUFFIX)',
          },
        }],
      ],
    }, {
      'variables': {
        'cargo_build_flags': [],
      },
      'conditions': [
        ['cargo_rust_target!=""', {
          'variables': {
            'node_crates_libpath': '<(SHARED_INTERMEDIATE_DIR)/$(Platform)/debug/node_crates.lib',
          },
        }, {
          'variables': {
            'node_crates_libpath': '<(SHARED_INTERMEDIATE_DIR)/debug/<(STATIC_LIB_PREFIX)node_crates<(STATIC_LIB_SUFFIX)',
          },
        }],
      ],
    }]
  ],
  'targets': [
    {
      'target_name': 'node_crates',
      'type': 'none',
      'toolsets': ['host', 'target'],
      'hard_dependency': 1,
      'sources': [
        'Cargo.toml',
        'Cargo.lock',
        'src/lib.rs',
      ],
      'link_settings': {
        'libraries': [
          '<(node_crates_libpath)',
        ],
        'conditions': [
          ['OS=="win"', {
            'libraries': [
              '-lntdll',
              '-luserenv'
            ],
          }],
        ],
      },
      'conditions': [
        ['cargo_rust_target!=""', {
          'actions': [
            {
              'action_name': 'cargo_build',
              'inputs': [
                '<@(_sources)'
              ],
              'outputs': [
                '<(node_crates_libpath)'
              ],
              'action': [
                '<(python)',
                'cargo_build.py',
                '$(Platform)',
                '<(SHARED_INTERMEDIATE_DIR)',
                '<@(cargo_build_flags)',
                '--frozen',
              ],
            }
          ],
        }, {
          'actions': [
            {
              'action_name': 'cargo_build',
              'inputs': [
                '<@(_sources)'
              ],
              'outputs': [
                '<(node_crates_libpath)'
              ],
              'action': [
                '<(cargo)',
                'rustc',
                '<@(cargo_build_flags)',
                '--frozen',
                '--target-dir',
                '<(SHARED_INTERMEDIATE_DIR)'
              ],
            }
          ],
        }],
      ],
    },
    {
      'target_name': 'temporal_capi',
      'type': 'none',
      'toolsets': ['host', 'target'],
      'sources': [],
      'dependencies': [
        'node_crates',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(cargo_vendor_dir)/temporal_capi/bindings/cpp',
        ],
      },
    },
  ]
}
