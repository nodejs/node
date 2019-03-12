# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'program',
      'type': 'executable',
      'sources': [
        'program.c',
        'override_asm.asm',
      ],
      'rules': [
      {
        # Test that if there's a specific .asm rule, it overrides the
        # built in one on Windows.
        'rule_name': 'assembler',
        'msvs_cygwin_shell': 0,
        'extension': 'asm',
        'inputs': [
          'as.bat',
        ],
        'outputs': [
          'output.obj',
        ],
        'action': ['as.bat', 'lib1.c', '<(_outputs)'],
        'message': 'Building assembly file <(RULE_INPUT_PATH)',
        'process_outputs_as_sources': 1,
        },
      ],
    },
  ],
}
