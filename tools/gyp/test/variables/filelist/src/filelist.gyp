# Copyright (c) 2009 Google Inc. All rights reserved.
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
      'Astor',
      'Jingleheimer',
      'Jerome',
      'Schmidt',
      'Schultz',
    ],
    'names!': [
      'Astor',
    ],
    'names/': [
      ['exclude', 'Sch.*'],
      ['include', '.*dt'],
      ['exclude', 'Jer.*'],
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
          'inputs' : [
            '<(names_listfile)',
            '<!@(cat <(names_listfile))',
          ],
          'outputs': [
            'dummy_foo',
          ],
          'action': [
            'python', 'dummy.py', '<(names_listfile)',
          ],
        },
      ],
    },
    {
      'target_name': 'bar',
      'type': 'none',
      'sources': [
        'John',
        'Jacob',
        'Astor',
        'Jingleheimer',
        'Jerome',
        'Schmidt',
        'Schultz',
      ],
      'sources!': [
        'Astor',
      ],
      'sources/': [
        ['exclude', 'Sch.*'],
        ['include', '.*dt'],
        ['exclude', 'Jer.*'],
      ],
      'variables': {
        'sources_listfile': '<|(sources.txt <@(_sources))',
      },
      'actions': [
        {
          'action_name': 'test_action',
          'inputs' : [
            '<(sources_listfile)',
            '<!@(cat <(sources_listfile))',
          ],
          'outputs': [
            'dummy_foo',
          ],
          'action': [
            'python', 'dummy.py', '<(sources_listfile)',
          ],
        },
      ],
    },
  ],
}
