# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'main',
      'toolsets': ['host'],
      'type': 'executable',
      'sources': [
        'main.cc',
      ],
    },
    {
      'target_name': 'post',
      'toolsets': ['host'],
      'type': 'none',
      'dependencies': [
        'main',
      ],
      'sources': [
        # As this test is written it could easily be made into an action.
        # An acutal use case would have a number of these 'sources'.
        '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)main<(EXECUTABLE_SUFFIX)',
      ],
      'rules': [
        {
          'rule_name': 'generate_output',
          'extension': '<(EXECUTABLE_SUFFIX)',
          'outputs': [ '<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT)_output', ],
          'msvs_cygwin_shell': 0,
          'action': [
            '<(RULE_INPUT_PATH)',
            '<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT)_output',
          ],
          'message': 'Generating output for <(RULE_INPUT_ROOT)'
        },
      ],
    },
  ],
}
