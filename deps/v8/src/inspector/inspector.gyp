# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{ 'variables': {
    'protocol_path': '../../third_party/WebKit/Source/platform/inspector_protocol',
    'protocol_sources': [
      '<(SHARED_INTERMEDIATE_DIR)/inspector/Console.cpp',
      '<(SHARED_INTERMEDIATE_DIR)/inspector/Console.h',
      '<(SHARED_INTERMEDIATE_DIR)/inspector/Debugger.cpp',
      '<(SHARED_INTERMEDIATE_DIR)/inspector/Debugger.h',
      '<(SHARED_INTERMEDIATE_DIR)/inspector/HeapProfiler.cpp',
      '<(SHARED_INTERMEDIATE_DIR)/inspector/HeapProfiler.h',
      '<(SHARED_INTERMEDIATE_DIR)/inspector/Profiler.cpp',
      '<(SHARED_INTERMEDIATE_DIR)/inspector/Profiler.h',
      '<(SHARED_INTERMEDIATE_DIR)/inspector/public/Debugger.h',
      '<(SHARED_INTERMEDIATE_DIR)/inspector/public/Runtime.h',
      '<(SHARED_INTERMEDIATE_DIR)/inspector/Runtime.cpp',
      '<(SHARED_INTERMEDIATE_DIR)/inspector/Runtime.h',
    ]
  },
  'targets': [
    { 'target_name': 'inspector_protocol_sources',
      'type': 'none',
      'variables': {
        'jinja_module_files': [
          # jinja2/__init__.py contains version string, so sufficient for package
          '../third_party/jinja2/__init__.py',
          '../third_party/markupsafe/__init__.py',  # jinja2 dep
        ]
      },
      'actions': [
        {
          'action_name': 'generate_inspector_protocol_sources',
          'inputs': [
            # Source generator script.
            '<(protocol_path)/CodeGenerator.py',
            # Source code templates.
            '<(protocol_path)/Exported_h.template',
            '<(protocol_path)/Imported_h.template',
            '<(protocol_path)/TypeBuilder_h.template',
            '<(protocol_path)/TypeBuilder_cpp.template',
            # Protocol definition.
            'js_protocol.json',
          ],
          'outputs': [
            '<@(protocol_sources)',
          ],
          'action': [
            'python',
            '<(protocol_path)/CodeGenerator.py',
            '--protocol', 'js_protocol.json',
            '--string_type', 'String16',
            '--export_macro', 'PLATFORM_EXPORT',
            '--output_dir', '<(SHARED_INTERMEDIATE_DIR)/inspector',
            '--output_package', 'inspector',
            '--exported_dir', '<(SHARED_INTERMEDIATE_DIR)/inspector/public',
            '--exported_package', 'inspector/public',
          ],
          'message': 'Generating Inspector protocol backend sources from json definitions',
        },
      ]
    },
    { 'target_name': 'inspector_protocol',
      'type': 'static_library',
      'dependencies': [
        'inspector_protocol_sources',
      ],
      'include_dirs+': [
        '<(protocol_path)/../..',
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      'defines': [
        'V8_INSPECTOR_USE_STL',
      ],
      'msvs_disabled_warnings': [
        4267,  # Truncation from size_t to int.
        4305,  # Truncation from 'type1' to 'type2'.
        4324,  # Struct padded due to declspec(align).
        4714,  # Function marked forceinline not inlined.
        4800,  # Value forced to bool.
        4996,  # Deprecated function call.
      ],
      'sources': [
        '<@(protocol_sources)',
        '<(protocol_path)/Allocator.h',
        '<(protocol_path)/Array.h',
        '<(protocol_path)/BackendCallback.h',
        '<(protocol_path)/CodeGenerator.py',
        '<(protocol_path)/Collections.h',
        '<(protocol_path)/DispatcherBase.cpp',
        '<(protocol_path)/DispatcherBase.h',
        '<(protocol_path)/ErrorSupport.cpp',
        '<(protocol_path)/ErrorSupport.h',
        '<(protocol_path)/FrontendChannel.h',
        '<(protocol_path)/Maybe.h',
        '<(protocol_path)/Object.cpp',
        '<(protocol_path)/Object.h',
        '<(protocol_path)/Parser.cpp',
        '<(protocol_path)/Parser.h',
        '<(protocol_path)/Platform.h',
        '<(protocol_path)/PlatformSTL.h',
        '<(protocol_path)/String16.cpp',
        '<(protocol_path)/String16.h',
        '<(protocol_path)/String16STL.cpp',
        '<(protocol_path)/String16STL.h',
        '<(protocol_path)/ValueConversions.h',
        '<(protocol_path)/Values.cpp',
        '<(protocol_path)/Values.h',
      ]
    },
  ],
}
