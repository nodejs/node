# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This one tests that assembly files ended as .s and .S are compiled.

{
  'target_defaults': {
    'conditions': [
      ['OS=="win"', {
        'defines': ['PLATFORM_WIN'],
      }],
      ['OS=="mac"', {
        'defines': ['PLATFORM_MAC'],
      }],
      ['OS=="linux"', {
        'defines': ['PLATFORM_LINUX'],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'program4',
      'type': 'executable',
      'sources': [
        'asm-function.assem',
        'program.c',
      ],
      'conditions': [
        ['OS=="linux" or OS=="mac"', {
          'rules': [
            {
              'rule_name': 'convert_assem',
              'extension': 'assem',
              'inputs': [],
              'outputs': [
                '<(SHARED_INTERMEDIATE_DIR)/<(RULE_INPUT_ROOT).S',
              ],
              'action': [
                'bash', '-c', 'cp <(RULE_INPUT_PATH) <@(_outputs)',
              ],
              'process_outputs_as_sources': 1,
            },
          ],
        }],
      ],
    },
  ],
}
