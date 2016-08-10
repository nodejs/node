# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'blink_platform_output_dir': '<(SHARED_INTERMEDIATE_DIR)/blink/platform',
  },
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
    {
      # GN version: //third_party/WebKit/Source/platform:inspector_protocol_sources
      'target_name': 'protocol_sources',
      'type': 'none',
      'dependencies': ['protocol_version'],
      'variables': {
        'conditions': [
          ['debug_devtools=="node"', {
              # Node build
              'jinja_module_files': [
                '../../../jinja2/jinja2/__init__.py',
                '../../../markupsafe/markupsafe/__init__.py',  # jinja2 dep
              ],
            }, {
              'jinja_module_files': [
                '<(DEPTH)/third_party/jinja2/__init__.py',
                '<(DEPTH)/third_party/markupsafe/__init__.py',  # jinja2 dep
              ],
            }
          ],
        ],
      },
      'actions': [
        {
          'action_name': 'generateV8InspectorProtocolBackendSources',
          'inputs': [
            '<@(jinja_module_files)',
            # The python script in action below.
            '../inspector_protocol/CodeGenerator.py',
            # Source code templates.
            '../inspector_protocol/TypeBuilder_h.template',
            '../inspector_protocol/TypeBuilder_cpp.template',
            '../inspector_protocol/Exported_h.template',
            '../inspector_protocol/Imported_h.template',
            # Protocol definitions
            'js_protocol.json',
          ],
          'outputs': [
            '<(blink_platform_output_dir)/v8_inspector/protocol/Console.cpp',
            '<(blink_platform_output_dir)/v8_inspector/protocol/Console.h',
            '<(blink_platform_output_dir)/v8_inspector/protocol/Debugger.cpp',
            '<(blink_platform_output_dir)/v8_inspector/protocol/Debugger.h',
            '<(blink_platform_output_dir)/v8_inspector/protocol/HeapProfiler.cpp',
            '<(blink_platform_output_dir)/v8_inspector/protocol/HeapProfiler.h',
            '<(blink_platform_output_dir)/v8_inspector/protocol/Profiler.cpp',
            '<(blink_platform_output_dir)/v8_inspector/protocol/Profiler.h',
            '<(blink_platform_output_dir)/v8_inspector/protocol/Runtime.cpp',
            '<(blink_platform_output_dir)/v8_inspector/protocol/Runtime.h',
            '<(blink_platform_output_dir)/v8_inspector/public/protocol/Runtime.h',
            '<(blink_platform_output_dir)/v8_inspector/public/protocol/Debugger.h',
          ],
          'action': [
            'python',
            '../inspector_protocol/CodeGenerator.py',
            '--protocol', 'js_protocol.json',
            '--string_type', 'String16',
            '--export_macro', 'PLATFORM_EXPORT',
            '--output_dir', '<(blink_platform_output_dir)/v8_inspector/protocol',
            '--output_package', 'platform/v8_inspector/protocol',
            '--exported_dir', '<(blink_platform_output_dir)/v8_inspector/public/protocol',
            '--exported_package', 'platform/v8_inspector/public/protocol',
          ],
          'message': 'Generating protocol backend sources from json definitions.',
        },
      ]
    },
    {
      # GN version: //third_party/WebKit/Source/core/inspector:protocol_version
      'target_name': 'protocol_version',
      'type': 'none',
      'actions': [
         {
          'action_name': 'generateV8InspectorProtocolVersion',
          'inputs': [
            '../inspector_protocol/generate-inspector-protocol-version',
            'js_protocol.json',
          ],
          'outputs': [
            '<(blink_platform_output_dir)/v8_inspector/protocol.json',
          ],
          'action': [
            'python',
            '../inspector_protocol/generate-inspector-protocol-version',
            '--o',
            '<@(_outputs)',
            'js_protocol.json',
          ],
          'message': 'Validate v8_inspector protocol for backwards compatibility and generate version file',
        },
      ]
    },
    {
      'target_name': 'v8_inspector_stl',
      'type': '<(component)',
      'dependencies': [
        ':inspector_injected_script',
        ':inspector_debugger_script',
        ':protocol_sources',
      ],
      'defines': [
        'V8_INSPECTOR_USE_STL=1'
      ],
      'include_dirs': [
        '../..',
        '../../../../../v8/include',
        '../../../../../v8',
        '<(SHARED_INTERMEDIATE_DIR)/blink',
      ],
      'sources': [
        '<(blink_platform_output_dir)/v8_inspector/protocol/Console.cpp',
        '<(blink_platform_output_dir)/v8_inspector/protocol/Console.h',
        '<(blink_platform_output_dir)/v8_inspector/protocol/Debugger.cpp',
        '<(blink_platform_output_dir)/v8_inspector/protocol/Debugger.h',
        '<(blink_platform_output_dir)/v8_inspector/protocol/HeapProfiler.cpp',
        '<(blink_platform_output_dir)/v8_inspector/protocol/HeapProfiler.h',
        '<(blink_platform_output_dir)/v8_inspector/protocol/Profiler.cpp',
        '<(blink_platform_output_dir)/v8_inspector/protocol/Profiler.h',
        '<(blink_platform_output_dir)/v8_inspector/protocol/Runtime.cpp',
        '<(blink_platform_output_dir)/v8_inspector/protocol/Runtime.h',

        '../inspector_protocol/Allocator.h',
        '../inspector_protocol/Array.h',
        '../inspector_protocol/Collections.h',
        '../inspector_protocol/DispatcherBase.cpp',
        '../inspector_protocol/DispatcherBase.h',
        '../inspector_protocol/ErrorSupport.cpp',
        '../inspector_protocol/ErrorSupport.h',
        '../inspector_protocol/Maybe.h',
        '../inspector_protocol/Parser.cpp',
        '../inspector_protocol/Parser.h',
        '../inspector_protocol/FrontendChannel.h',
        '../inspector_protocol/String16.h',
        '../inspector_protocol/String16STL.cpp',
        '../inspector_protocol/String16STL.h',
        '../inspector_protocol/Values.cpp',
        '../inspector_protocol/Values.h',
        '../inspector_protocol/ValueConversions.h',

        'Atomics.h',
        'InjectedScript.cpp',
        'InjectedScript.h',
        'InjectedScriptNative.cpp',
        'InjectedScriptNative.h',
        'InspectedContext.cpp',
        'InspectedContext.h',
        'JavaScriptCallFrame.cpp',
        'JavaScriptCallFrame.h',
        'MuteConsoleScope.h',
        'ScriptBreakpoint.h',
        'RemoteObjectId.cpp',
        'RemoteObjectId.h',
        'V8Console.cpp',
        'V8Console.h',
        'V8ConsoleAgentImpl.cpp',
        'V8ConsoleAgentImpl.h',
        'V8ConsoleMessage.cpp',
        'V8ConsoleMessage.h',
        'V8Debugger.cpp',
        'V8Debugger.h',
        'V8DebuggerAgentImpl.cpp',
        'V8DebuggerAgentImpl.h',
        'V8InspectorImpl.cpp',
        'V8InspectorImpl.h',
        'V8DebuggerScript.cpp',
        'V8DebuggerScript.h',
        'V8FunctionCall.cpp',
        'V8FunctionCall.h',
        'V8HeapProfilerAgentImpl.cpp',
        'V8HeapProfilerAgentImpl.h',
        'V8InjectedScriptHost.cpp',
        'V8InjectedScriptHost.h',
        'V8InspectorSessionImpl.cpp',
        'V8InspectorSessionImpl.h',
        'V8InternalValueType.cpp',
        'V8InternalValueType.h',
        'V8ProfilerAgentImpl.cpp',
        'V8ProfilerAgentImpl.h',
        'V8Regex.cpp',
        'V8Regex.h',
        'V8RuntimeAgentImpl.cpp',
        'V8RuntimeAgentImpl.h',
        'V8StackTraceImpl.cpp',
        'V8StackTraceImpl.h',
        'V8StringUtil.cpp',
        'V8StringUtil.h',
        'public/V8EventListenerInfo.h',
        'public/V8ContextInfo.h',
        'public/V8Inspector.h',
        'public/V8InspectorClient.h',
        'public/V8HeapProfilerAgent.h',
        'public/V8InspectorSession.h',
        'public/V8StackTrace.h',

        '<(blink_platform_output_dir)/v8_inspector/DebuggerScript.h',
        '<(blink_platform_output_dir)/v8_inspector/InjectedScriptSource.h',
      ],
    },
  ],  # targets
}
