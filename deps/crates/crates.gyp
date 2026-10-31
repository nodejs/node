{
  'variables': {
    'cargo%': 'cargo',
    'cargo_vendor_dir': './vendor',
    'temporal_capi_dir': 'temporal_capi-v0_2',
    'zoneinfo64_dir': 'zoneinfo64-v0_3',
    'temporal_zoneinfo64_data_output': '<(SHARED_INTERMEDIATE_DIR)/src/builtins/builtins-temporal-zoneinfo64-data.cc',
    'cargo_rust_target%': '',
    'cargo_cross_compiling%': 0,
    'cargo_rust_target_host%': '',
    'cargo_rust_target_target%': '',
    'node_shared_temporal_capi%': 'false',
  },
  'conditions': [
    ['build_type == "Release"', {
      'variables': {
        'cargo_build_flags': ['--release'],
        'cargo_profile': 'release',
      },
      'conditions': [
        ['cargo_rust_target!="" and cargo_cross_compiling==0', {
          'variables': {
            'cargo_build_flags': ['--target', '<(cargo_rust_target)'],
          }
        }],
        ['OS=="win"', {
          'variables': {
            'node_crates_libpath': '<(SHARED_INTERMEDIATE_DIR)/$(Platform)/release/node_crates.lib',
          },
        }, {
          'variables': {
            'node_crates_libpath': '<(SHARED_INTERMEDIATE_DIR)/<(cargo_rust_target)/release/<(STATIC_LIB_PREFIX)node_crates<(STATIC_LIB_SUFFIX)',
          },
        }],
      ],
    }, {
      'variables': {
        'cargo_build_flags': [],
        'cargo_profile': 'debug',
      },
      'conditions': [
        ['OS=="win"', {
          'variables': {
            'node_crates_libpath': '<(SHARED_INTERMEDIATE_DIR)/$(Platform)/debug/node_crates.lib',
          },
        }, {
          'variables': {
            'node_crates_libpath': '<(SHARED_INTERMEDIATE_DIR)/<(cargo_rust_target)/debug/<(STATIC_LIB_PREFIX)node_crates<(STATIC_LIB_SUFFIX)',
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
      'variables': {
        'cargo_output_dir': '<(SHARED_INTERMEDIATE_DIR)',
        'cargo_cross_target_flags': [],
        'conditions': [
          ['cargo_cross_compiling==1', {
            'cargo_output_dir': '<(SHARED_INTERMEDIATE_DIR)/cargo/<(_toolset)',
            'cargo_cross_target_flags': ['--target', '<(cargo_rust_target_<(_toolset))'],
            'node_crates_libpath': '<(SHARED_INTERMEDIATE_DIR)/cargo/<(_toolset)/<(cargo_rust_target_<(_toolset))/<(cargo_profile)/<(STATIC_LIB_PREFIX)node_crates<(STATIC_LIB_SUFFIX)',
          }],
        ],
      },
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
        ['OS=="win" and node_shared_temporal_capi=="false"', {
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
        }],
        ['OS!="win" and node_shared_temporal_capi=="false"', {
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
                '<@(cargo_cross_target_flags)',
                '<@(cargo_build_flags)',
                '--frozen',
                '--target-dir',
                '<(cargo_output_dir)'
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
          '<(cargo_vendor_dir)/<(temporal_capi_dir)/bindings/cpp',
        ],
      },
    },
    {
      # Bakes zoneinfo64.res into a C++ source for Temporal when ICU is not
      # available to load it at runtime.
      'target_name': 'temporal_zoneinfo64_data',
      'type': 'none',
      'toolsets': ['host', 'target'],
      'hard_dependency': 1,
      'direct_dependent_settings': {
        'sources': [
          '<(temporal_zoneinfo64_data_output)',
        ],
      },
      'actions': [
        {
          'action_name': 'make_temporal_zoneinfo_cpp',
          'inputs': [
            '../v8/tools/include-file-as-bytes.py',
            '<(cargo_vendor_dir)/<(zoneinfo64_dir)/src/data/zoneinfo64.res',
          ],
          'outputs': [
            '<(temporal_zoneinfo64_data_output)',
          ],
          'action': [
            '<(python)',
            '<@(_inputs)',
            '<@(_outputs)',
            'zoneinfo64_static_data',
          ],
        },
      ],
    },
  ]
}
