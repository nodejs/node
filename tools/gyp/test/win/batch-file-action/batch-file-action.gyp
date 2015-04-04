# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_batch',
      'type': 'none',
      'actions': [
        {
          'action_name': 'copy_to_output',
          'inputs': ['infile'],
          'outputs': ['outfile'],
          'action': ['somecmd.bat', 'infile', 'outfile'],
          'msvs_cygwin_shell': 0,
        }
      ],
    },
  ]
}
