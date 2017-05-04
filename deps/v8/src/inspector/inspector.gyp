# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'protocol_path': '../../third_party/inspector_protocol',
  },
  'includes': [
    'inspector.gypi',
    '<(PRODUCT_DIR)/../../../third_party/inspector_protocol/inspector_protocol.gypi',
  ],
  'targets': [
    { 'target_name': 'inspector_injected_script',
      'type': 'none',
      'toolsets': ['target'],
      'actions': [
        {
          'action_name': 'convert_js_to_cpp_char_array',
          'inputs': [
            'build/xxd.py',
            '<(inspector_injected_script_source)',
          ],
          'outputs': [
            '<(inspector_generated_injected_script)',
          ],
          'action': [
            'python',
            'build/xxd.py',
            'InjectedScriptSource_js',
            'injected-script-source.js',
            '<@(_outputs)'
          ],
        },
      ],
      # Since this target generates header files, it needs to be a hard dependency.
      'hard_dependency': 1,
    },
    { 'target_name': 'inspector_debugger_script',
      'type': 'none',
      'toolsets': ['target'],
      'actions': [
        {
          'action_name': 'convert_js_to_cpp_char_array',
          'inputs': [
            'build/xxd.py',
            '<(inspector_debugger_script_source)',
          ],
          'outputs': [
            '<(inspector_generated_debugger_script)',
          ],
          'action': [
            'python',
            'build/xxd.py',
            'DebuggerScript_js',
            'debugger-script.js',
            '<@(_outputs)'
          ],
        },
      ],
      # Since this target generates header files, it needs to be a hard dependency.
      'hard_dependency': 1,
    },
    { 'target_name': 'protocol_compatibility',
      'type': 'none',
      'toolsets': ['target'],
      'actions': [
        {
          'action_name': 'protocol_compatibility',
          'inputs': [
            'js_protocol.json',
          ],
          'outputs': [
            '<@(SHARED_INTERMEDIATE_DIR)/src/js_protocol.stamp',
          ],
          'action': [
            'python',
            '<(protocol_path)/CheckProtocolCompatibility.py',
            '--stamp', '<@(_outputs)',
            'js_protocol.json',
          ],
          'message': 'Generating inspector protocol sources from protocol json definition',
        },
      ]
    },
    { 'target_name': 'protocol_generated_sources',
      'type': 'none',
      'dependencies': [ 'protocol_compatibility' ],
      'toolsets': ['target'],
      'actions': [
        {
          'action_name': 'protocol_generated_sources',
          'inputs': [
            'js_protocol.json',
            'inspector_protocol_config.json',
            '<@(inspector_protocol_files)',
          ],
          'outputs': [
            '<@(inspector_generated_sources)',
          ],
          'action': [
            'python',
            '<(protocol_path)/CodeGenerator.py',
            '--jinja_dir', '../../third_party',
            '--output_base', '<(SHARED_INTERMEDIATE_DIR)/src/inspector',
            '--config', 'inspector_protocol_config.json',
          ],
          'message': 'Generating inspector protocol sources from protocol json',
        },
      ]
    },
  ],
}
