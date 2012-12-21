# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_batch_depth',
      'type': 'none',
      'variables': {
        # Taken from native_client/build/common.gypi. Seems unintentional (a
        # string in a 1 element list)? But since it works on other generators,
        # I guess it should work here too.
        'filepath': [ 'call <(DEPTH)/../../../go.bat' ],
      },
      'rules': [
      {
        'rule_name': 'build_with_batch4',
        'msvs_cygwin_shell': 0,
        'extension': 'S',
        'inputs': ['<(RULE_INPUT_PATH)'],
        'outputs': ['output4.obj'],
        'action': ['<@(filepath)', '<(RULE_INPUT_PATH)', 'output4.obj'],
      },],
      'sources': ['<(DEPTH)\\..\\..\\..\\a.S'],
    },
  ]
}
