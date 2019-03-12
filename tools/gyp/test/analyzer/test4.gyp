# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'a',
      'type': 'executable',
      'sources': [
        'a.c',
      ],
      'dependencies': [
        'b',
        'c',
      ],
    },
    {
      'target_name': 'b',
      'type': 'executable',
      'sources': [
        'b.c',
      ],
      'dependencies': [
        'd',
      ],
    },
    {
      'target_name': 'c',
      'type': 'executable',
      'sources': [
        'c.c',
      ],
      'dependencies': [
        'b',
        'd',
      ],
    },
    {
      'target_name': 'd',
      'type': 'executable',
      'sources': [
        'd.c',
      ],
    },
    {
      'target_name': 'e',
      'type': 'executable',
      'dependencies': [
        'test5.gyp:f',
      ],
    },
    {
      'target_name': 'h',
      'type': 'none',
      'dependencies': [
        'i',
      ],
      'rules': [
        {
          'rule_name': 'rule',
          'extension': 'pdf',
          'inputs': [
            'rule_input.c',
          ],
          'outputs': [
            'rule_output.pdf',
          ],
        },
      ],
    },
    {
      'target_name': 'i',
      'type': 'static_library',
      'sources': [
        'i.c',
      ],
    },
  ],
}
