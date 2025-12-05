{
  'variables': {
    'cargo_vendor_dir': './vendor',
  },
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
          '<(SHARED_INTERMEDIATE_DIR)/>(cargo_build_mode)/libnode_crates.a',
        ],
      },
      'actions': [
        {
          'action_name': 'cargo_build',
          'inputs': [
            '<@(_sources)'
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/>(cargo_build_mode)/libnode_crates.a'
          ],
          'action': [
            'cargo',
            'rustc',
            '>@(cargo_build_flags)',
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
