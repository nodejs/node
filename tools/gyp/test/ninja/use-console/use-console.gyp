# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'no_pool',
      'type': 'none',
      'actions': [
        {
          'action_name': 'some_action',
          'action': ['echo', 'hello'],
          'inputs': ['foo.bar'],
          'outputs': ['dummy'],
        },
      ],
      'rules': [
        {
          'rule_name': 'some_rule',
          'extension': 'bar',
          'action': ['echo', 'hello'],
          'outputs': ['dummy'],
        },
      ],
      'sources': [
        'foo.bar',
      ],
    },
    {
      'target_name': 'action_pool',
      'type': 'none',
      'actions': [
        {
          'action_name': 'some_action',
          'action': ['echo', 'hello'],
          'inputs': ['foo.bar'],
          'outputs': ['dummy'],
          'ninja_use_console': 1,
        },
      ],
    },
    {
      'target_name': 'rule_pool',
      'type': 'none',
      'rules': [
        {
          'rule_name': 'some_rule',
          'extension': 'bar',
          'action': ['echo', 'hello'],
          'outputs': ['dummy'],
          'ninja_use_console': 1,
        },
      ],
      'sources': [
        'foo.bar',
      ],
    },
  ],
}
