# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_expansions',
      'msvs_cygwin_shell': 0,
      'type': 'none',
      'rules': [
        {
          'rule_name': 'assembler (gnu-compatible)',
          'msvs_cygwin_shell': 0,
          'msvs_quote_cmd': 0,
          'extension': 'S',
          'inputs': [
            'as.py',
            '$(InputPath)'
          ],
          'outputs': [
            '$(IntDir)/$(InputName).obj',
          ],
          'action':
            ['python',
              'as.py',
              '-a', '$(PlatformName)',
              '-o', '$(IntDir)/$(InputName).obj',
              '-p', '<(DEPTH)',
              '$(InputPath)'],
          'message': 'Building assembly language file $(InputPath)',
          'process_outputs_as_sources': 1,
        },
      ],
      'sources': [
        'input.S',
      ],
    },
  ]
}
