# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'lib32or64',
      'type': 'static_library',
      'toolsets': [ 'host' ],
      'sources': [
        '32or64.c',
      ],
    },
    {
      'target_name': 'host_32or64',
      'type': 'executable',
      'toolsets': [ 'host' ],
      'dependencies': [ 'lib32or64#host' ],
      'sources': [
        'writefile.c',
      ],
    },
    {
      'target_name': 'generate_output',
      'type': 'none',
      'dependencies': [ 'host_32or64#host' ],
      'actions': [
        {
          'action_name': 'generate',
          'inputs': [ '<(PRODUCT_DIR)/host_32or64' ],
          'outputs': [ '<(PRODUCT_DIR)/host_32or64.output' ],
          'action': [ '<@(_inputs)', '<@(_outputs)' ],
        }
      ],
    },
  ],
}
