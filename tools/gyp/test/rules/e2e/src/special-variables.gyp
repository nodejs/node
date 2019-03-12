# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'rules': [
        {
          'rule_name': 'assembler (gnu-compatible)',
          'msvs_cygwin_shell': 0,
          'msvs_quote_cmd': 0,
          'extension': 'S',
          'inputs': [
            'as.bat',
          ],
          'outputs': [
            '$(IntDir)/$(InputName).obj',
          ],
          'action': [
            'as.bat',
            '$(InputPath)',
            '$(IntDir)/$(InputName).obj',
          ],
          'message': 'Building assembly language file $(InputPath)',
          'process_outputs_as_sources': 1,
        },
      ],
      'target_name': 'test',
      'type': 'static_library',
      'sources': [ 'an_asm.S' ],
    },
  ],
}
