{
  'targets': [
    {
      'target_name': 'temporal_capi',
      'type': 'none',
      'hard_dependency': 1,
      'sources': [
        'src/calendar.rs',
        'src/error.rs',
        'src/lib.rs',
        'src/plain_date_time.rs',
        'src/plain_month_day.rs',
        'src/plain_year_month.rs',
        'src/time_zone.rs',
        'src/duration.rs',
        'src/instant.rs',
        'src/options.rs',
        'src/plain_date.rs',
        'src/plain_time.rs',
        'src/provider.rs',
        'src/zoned_date_time.rs',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'bindings/cpp',
        ],
      },
      'link_settings': {
        'libraries': [
          '<(SHARED_INTERMEDIATE_DIR)/>(cargo_build_mode)/libtemporal_capi.a',
        ],
      },
      'actions': [
        {
          'action_name': 'cargo_build',
          'inputs': [
            '<@(_sources)',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/>(cargo_build_mode)/libtemporal_capi.a'
          ],
          'action': [
            'cargo',
            'rustc',
            '>@(cargo_build_flags)',
            '--crate-type',
            'staticlib',
            '--features',
            'zoneinfo64',
            '--target-dir',
            '<(SHARED_INTERMEDIATE_DIR)'
          ],
        }
      ],
    }
  ]
}
