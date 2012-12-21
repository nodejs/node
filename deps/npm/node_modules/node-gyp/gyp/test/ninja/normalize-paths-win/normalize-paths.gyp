# Copyright (c) 2010 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'Some_Target',
      'type': 'executable',
      'msvs_settings': {
        'VCLinkerTool': {
          'OutputFile': '<(PRODUCT_DIR)/stuff/AnotherName.exe',
        },
      },
      'sources': [
        'HeLLo.cc',
        'blOrP.idl',
      ],
    },
    {
      'target_name': 'second',
      'type': 'executable',
      'msvs_settings': {
        'VCLinkerTool': {
          'OutputFile': '$(OutDir)\\things\\AnotherName.exe',
        },
      },
      'sources': [
        'HeLLo.cc',
      ],
    },
    {
      'target_name': 'action',
      'type': 'none',
      'msvs_cygwin_shell': '0',
      'actions': [
        {
          'inputs': [
            '$(IntDir)\\SomeInput',
            '$(OutDir)\\SomeOtherInput',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/ReSuLt',
            '<(SHARED_INTERMEDIATE_DIR)/TempFile',
            '$(OutDir)\SomethingElse',
          ],
          'action_name': 'Test action',
          # Unfortunately, we can't normalize this field because it's
          # free-form. Fortunately, ninja doesn't inspect it at all (only the
          # inputs and outputs) so it's not mandatory.
          'action': [],
        },
      ],
    },
  ],
}
