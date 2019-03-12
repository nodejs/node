# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'print_rule_input_dirname',
      'type': 'none',
      'msvs_cygwin_shell': 0,
      'sources': [
        'foo/bar/baz.printvars',
        'a/b/c.printvars',
      ],
      'rules': [
        {
          'rule_name': 'printvars',
          'extension': 'printvars',
          'inputs': [
            'printvars.py',
          ],
          'outputs': [
            '<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).dirname',
          ],
          'action': [
            'python', '<@(_inputs)', '<(RULE_INPUT_DIRNAME)', '<@(_outputs)',
          ],
        },
      ],
    },
    {
      'target_name': 'print_rule_input_path',
      'type': 'none',
      'msvs_cygwin_shell': 0,
      'sources': [
        'foo/bar/baz.printvars',
        'a/b/c.printvars',
      ],
      'rules': [
        {
          'rule_name': 'printvars',
          'extension': 'printvars',
          'inputs': [
            'printvars.py',
          ],
          'outputs': [
            '<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).path',
          ],
          'action': [
            'python', '<@(_inputs)', '<(RULE_INPUT_PATH)', '<@(_outputs)',
          ],
        },
      ],
    },
    {
      'target_name': 'gencc_int_output',
      'type': 'executable',
      'msvs_cygwin_shell': 0,
      'sources': [
        'nodir.gencc',
        'foo/bar/baz.gencc',
        'a/b/c.gencc',
        'main.cc',
      ],
      'rules': [
        {
          'rule_name': 'gencc',
          'extension': 'gencc',
          'inputs': [
            '<(DEPTH)/copy-file.py',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).cc',
          ],
          'action': [
            'python', '<@(_inputs)', '<(RULE_INPUT_PATH)', '<@(_outputs)',
          ],
          'process_outputs_as_sources': 1,
        },
      ],
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'gencc_int_output_external',
          'type': 'executable',
          'msvs_cygwin_shell': 0,
          'msvs_cygwin_dirs': ['../../../../../../<(DEPTH)/third_party/cygwin'],
          'sources': [
            'nodir.gencc',
            'foo/bar/baz.gencc',
            'a/b/c.gencc',
            'main.cc',
          ],
          'dependencies': [
            'cygwin',
          ],
          'rules': [
            {
              'rule_name': 'gencc',
              'extension': 'gencc',
              'msvs_external_rule': 1,
              'inputs': [
                '<(DEPTH)/copy-file.py',
              ],
              'outputs': [
                '<(INTERMEDIATE_DIR)/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).cc',
              ],
              'action': [
                'python', '<@(_inputs)', '<(RULE_INPUT_PATH)', '<@(_outputs)',
              ],
              'process_outputs_as_sources': 1,
            },
          ],
        },
        {
          'target_name': 'cygwin',
          'type': 'none',
          'actions': [
            {
              'action_name': 'setup_mount',
              'msvs_cygwin_shell': 0,
              'inputs': [
                '../../../../../../<(DEPTH)/third_party/cygwin/setup_mount.bat',
              ],
              # Visual Studio requires an output file, or else the
              # custom build step won't run.
              'outputs': [
                '<(INTERMEDIATE_DIR)/_always_run_setup_mount.marker',
              ],
              'action': ['<@(_inputs)'],
            },
          ],
        },
      ],
    }],
  ],
}
