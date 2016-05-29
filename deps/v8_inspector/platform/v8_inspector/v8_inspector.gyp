# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //third_party/WebKit/Source/platform:inspector_injected_script
      'target_name': 'inspector_injected_script',
      'type': 'none',
      'actions': [
        {
          'action_name': 'ConvertFileToHeaderWithCharacterArray',
          'inputs': [
            'build/xxd.py',
            'InjectedScriptSource.js',
          ],
          'outputs': [ '<(SHARED_INTERMEDIATE_DIR)/blink/platform/v8_inspector/InjectedScriptSource.h', ],
          'action': [
            'python', 'build/xxd.py', 'InjectedScriptSource_js', 'InjectedScriptSource.js', '<@(_outputs)'
          ],
        },
      ],
      # Since this target generates header files, it needs to be a hard dependency.
      'hard_dependency': 1,
    },
    {
      # GN version: //third_party/WebKit/Source/platform:inspector_debugger_script
      'target_name': 'inspector_debugger_script',
      'type': 'none',
      'actions': [
        {
          'action_name': 'ConvertFileToHeaderWithCharacterArray',
          'inputs': [
            'build/xxd.py',
            'DebuggerScript.js',
          ],
          'outputs': [ '<(SHARED_INTERMEDIATE_DIR)/blink/platform/v8_inspector/DebuggerScript.h', ],
          'action': [
            'python', 'build/xxd.py', 'DebuggerScript_js', 'DebuggerScript.js', '<@(_outputs)'
          ],
        },
      ],
      # Since this target generates header files, it needs to be a hard dependency.
      'hard_dependency': 1,
    },
  ],  # targets
}
