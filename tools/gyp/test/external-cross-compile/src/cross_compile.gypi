{
  'target_defaults': {
    'variables': {
      'cross%': 0,
    },
    'target_conditions': [
      ['cross==1', {
        'actions': [
          {
            'action_name': 'cross compile >(_target_name)',
            'inputs': ['^@(_sources)'],
            'outputs': ['<(SHARED_INTERMEDIATE_DIR)/>(_target_name).fake'],
            'action': [
              'python', 'fake_cross.py', '>@(_outputs)', '^@(_sources)',
            ],
            # Allows the test to run without hermetic cygwin on windows.
            'msvs_cygwin_shell': 0,
          },
        ],
      }],
    ],
  },
}
