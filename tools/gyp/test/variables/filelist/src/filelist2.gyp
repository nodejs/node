# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is a test to make sure that <|(foo.txt a b c) generates
# a pre-calculated file list at gyp time and returns foo.txt.
# This feature is useful to work around limits in the number of arguments that
# can be passed to rule/action.

{
  'variables': {
    'names': [
      'John',
      'Jacob',
      'Jingleheimer',
      'Schmidt',
    ],
  },
  'targets': [
    {
      'target_name': 'foo',
      'type': 'none',
      'variables': {
        'names_listfile': '<|(names.txt <@(names))',
      },
      'actions': [
        {
          'action_name': 'test_action',
          'msvs_cygwin_shell': 0,
          'inputs' : [ '<(names_listfile)' ],
          'outputs': [ 'dummy_foo' ],
          'action': [
            'python', 'dummy.py', '<@(_outputs)', '<(names_listfile)',
          ],
        },
      ],
    },
  ],
}

