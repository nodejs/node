{
  'variables': {
    'cargo_vendor_dir': './vendor',
  },
  'conditions': [
    ['build_type == "Release"', {
      'variables': {
        'cargo_build_flags': ['--release'],
        'node_crates_libpath': '<(SHARED_INTERMEDIATE_DIR)/release/<(STATIC_LIB_PREFIX)node_crates<(STATIC_LIB_SUFFIX)',
      },
    }, {
      'variables': {
        'cargo_build_flags': [],
        'node_crates_libpath': '<(SHARED_INTERMEDIATE_DIR)/debug/<(STATIC_LIB_PREFIX)node_crates<(STATIC_LIB_SUFFIX)',
      },
    }]
  ],
  'targets': [
    {
      'target_name': 'node_crates',
      'type': 'none',
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
            'cargo',
            'rustc',
            '<@(cargo_build_flags)',
            '--frozen',
            '--target-dir',
            '<(SHARED_INTERMEDIATE_DIR)'
          ],
        }
      ],
    },
    {
      'target_name': 'temporal_capi',
      'type': 'none',
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
