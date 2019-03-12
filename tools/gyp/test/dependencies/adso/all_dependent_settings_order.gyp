{
  'targets': [
    {
      'target_name': 'a',
      'type': 'none',
      'sources': ['a.cc'],
      'all_dependent_settings': {'sources': ['a.cc']},
    },
    {
      'target_name': 'b',
      'type': 'none',
      'sources': ['b.cc'],
      'all_dependent_settings': {'sources': ['b.cc']},
      'dependencies': ['a'],
    },

    {
      'target_name': 'c',
      'type': 'none',
      'sources': ['c.cc'],
      'all_dependent_settings': {'sources': ['c.cc']},
      'dependencies': ['b', 'a'],
    },
    {
      'target_name': 'd',
      'type': 'none',
      'sources': ['d.cc'],
      'dependencies': ['c', 'a', 'b'],
      'actions': [
        {
          'action_name': 'write_sources',
          'inputs': ['write_args.py'],
          'outputs': ['<(PRODUCT_DIR)/out.txt'],
          'action': [
            'python',
            'write_args.py',
            '<(PRODUCT_DIR)/out.txt',
            '>@(_sources)'
          ],
          'msvs_cygwin_shell': 0,
        },
      ],
    },
  ],
}
