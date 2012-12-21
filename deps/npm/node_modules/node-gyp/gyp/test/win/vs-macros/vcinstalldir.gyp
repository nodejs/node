# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_slash_trailing',
      'type': 'none',
      'msvs_cygwin_shell': '0',
      'actions': [
        {
          'action_name': 'root',
          'inputs': [],
          'outputs': ['out1'],
          'action': ['python', 'test_exists.py', '$(VCInstallDir)', 'out1']
        },
      ],
    },
    {
      'target_name': 'test_slash_dir',
      'type': 'none',
      'msvs_cygwin_shell': '0',
      'actions': [
        {
          'action_name': 'bin',
          'inputs': [],
          'outputs': ['out2'],
          'action': ['python', 'test_exists.py', '$(VCInstallDir)bin', 'out2'],
        },
        {
          'action_name': 'compiler',
          'inputs': [],
          'outputs': ['out3'],
          'action': [
              'python', 'test_exists.py', '$(VCInstallDir)bin\\cl.exe', 'out3'],
        },
      ],
    },
  ]
}
