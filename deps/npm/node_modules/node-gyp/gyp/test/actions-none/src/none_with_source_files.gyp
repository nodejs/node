# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Test that 'none' type targets can have .cc files in them.

{
  'targets': [
    {
      'target_name': 'none_with_sources',
      'type': 'none',
      'msvs_cygwin_shell': 0,
      'sources': [
        'foo.cc',
      ],
      'actions': [
        {
          'action_name': 'fake_cross',
          'inputs': [
            'fake_cross.py',
            '<@(_sources)',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/fake.out',
          ],
          'action': [
            'python', '<@(_inputs)', '<@(_outputs)',
          ],
          # Allows the test to run without hermetic cygwin on windows.
          'msvs_cygwin_shell': 0,
        }
      ],
    },
  ],
}
