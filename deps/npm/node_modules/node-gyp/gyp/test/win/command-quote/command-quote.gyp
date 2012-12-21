# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'msvs_cygwin_dirs': ['../../../../../<(DEPTH)/third_party/cygwin'],
  },
  'targets': [
    {
      'target_name': 'test_batch',
      'type': 'none',
      'rules': [
      {
        'rule_name': 'build_with_batch',
        'msvs_cygwin_shell': 0,
        'extension': 'S',
        'inputs': ['<(RULE_INPUT_PATH)'],
        'outputs': ['output.obj'],
        'action': ['call go.bat', '<(RULE_INPUT_PATH)', 'output.obj'],
      },],
      'sources': ['a.S'],
    },
    {
      'target_name': 'test_call_separate',
      'type': 'none',
      'rules': [
      {
        'rule_name': 'build_with_batch2',
        'msvs_cygwin_shell': 0,
        'extension': 'S',
        'inputs': ['<(RULE_INPUT_PATH)'],
        'outputs': ['output2.obj'],
        'action': ['call', 'go.bat', '<(RULE_INPUT_PATH)', 'output2.obj'],
      },],
      'sources': ['a.S'],
    },
    {
      'target_name': 'test_with_spaces',
      'type': 'none',
      'rules': [
      {
        'rule_name': 'build_with_batch3',
        'msvs_cygwin_shell': 0,
        'extension': 'S',
        'inputs': ['<(RULE_INPUT_PATH)'],
        'outputs': ['output3.obj'],
        'action': ['bat with spaces.bat', '<(RULE_INPUT_PATH)', 'output3.obj'],
      },],
      'sources': ['a.S'],
    },
    {
      'target_name': 'test_with_double_quotes',
      'type': 'none',
      'rules': [
      {
        'rule_name': 'build_with_batch3',
        'msvs_cygwin_shell': 1,
        'extension': 'S',
        'inputs': ['<(RULE_INPUT_PATH)'],
        'outputs': ['output4.obj'],
        'arguments': ['-v'],
        'action': ['python', '-c', 'import shutil; '
          'shutil.copy("<(RULE_INPUT_PATH)", "output4.obj")'],
      },],
      'sources': ['a.S'],
    },
    {
      'target_name': 'test_with_single_quotes',
      'type': 'none',
      'rules': [
      {
        'rule_name': 'build_with_batch3',
        'msvs_cygwin_shell': 1,
        'extension': 'S',
        'inputs': ['<(RULE_INPUT_PATH)'],
        'outputs': ['output5.obj'],
        'action': ['python', '-c', "import shutil; "
          "shutil.copy('<(RULE_INPUT_PATH)', 'output5.obj')"],
      },],
      'sources': ['a.S'],
    },
  ]
}
