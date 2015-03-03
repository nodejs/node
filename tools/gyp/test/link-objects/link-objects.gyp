# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'link-objects',
      'type': 'executable',
      'actions': [
        {
          'action_name': 'build extra object',
          'inputs': ['extra.c'],
          'outputs': ['extra.o'],
          'action': ['gcc', '-o', 'extra.o', '-c', 'extra.c'],
          'process_outputs_as_sources': 1,
        },
      ],
      'sources': [
        'base.c',
      ],
    },
  ],
}
