{
  'targets': [
    {
      'target_name': 'program',
      'type': 'executable',
      'msvs_cygwin_shell': 0,
      'sources': [
        'main.c',
        'prog1.in',
        'prog2.in',
      ],
      'rules': [
        {
          'rule_name': 'make_sources',
          'extension': 'in',
          'inputs': [
            'make-sources.py',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/<(RULE_INPUT_ROOT).c',
            '<(INTERMEDIATE_DIR)/<(RULE_INPUT_ROOT).h',
          ],
          'action': [
            'python', '<(_inputs)', '<(RULE_INPUT_NAME)', '<@(_outputs)',
          ],
          'process_outputs_as_sources': 1,
        },
      ],
    },
  ],
}
