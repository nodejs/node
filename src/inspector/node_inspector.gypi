{
  'variables': {
    'protocol_tool_path': '../../tools/inspector_protocol',
    'node_inspector_path': './',
    'node_inspector_generated_path': '<(SHARED_INTERMEDIATE_DIR)/node_inspector',
    'node_inspector_generated_sources': [
      '<(node_inspector_generated_path)/protocol/Forward.h',
      '<(node_inspector_generated_path)/protocol/Protocol.cpp',
      '<(node_inspector_generated_path)/protocol/Protocol.h',
      '<(node_inspector_generated_path)/protocol/NodeWorker.cpp',
      '<(node_inspector_generated_path)/protocol/NodeWorker.h',
      '<(node_inspector_generated_path)/protocol/NodeTracing.cpp',
      '<(node_inspector_generated_path)/protocol/NodeTracing.h',
    ],
    'node_protocol_files': [
      '<(protocol_tool_path)/lib/Allocator_h.template',
      '<(protocol_tool_path)/lib/Array_h.template',
      '<(protocol_tool_path)/lib/Collections_h.template',
      '<(protocol_tool_path)/lib/DispatcherBase_cpp.template',
      '<(protocol_tool_path)/lib/DispatcherBase_h.template',
      '<(protocol_tool_path)/lib/ErrorSupport_cpp.template',
      '<(protocol_tool_path)/lib/ErrorSupport_h.template',
      '<(protocol_tool_path)/lib/Forward_h.template',
      '<(protocol_tool_path)/lib/FrontendChannel_h.template',
      '<(protocol_tool_path)/lib/Maybe_h.template',
      '<(protocol_tool_path)/lib/Object_cpp.template',
      '<(protocol_tool_path)/lib/Object_h.template',
      '<(protocol_tool_path)/lib/Parser_cpp.template',
      '<(protocol_tool_path)/lib/Parser_h.template',
      '<(protocol_tool_path)/lib/Protocol_cpp.template',
      '<(protocol_tool_path)/lib/ValueConversions_h.template',
      '<(protocol_tool_path)/lib/Values_cpp.template',
      '<(protocol_tool_path)/lib/Values_h.template',
      '<(protocol_tool_path)/templates/Exported_h.template',
      '<(protocol_tool_path)/templates/Imported_h.template',
      '<(protocol_tool_path)/templates/TypeBuilder_cpp.template',
      '<(protocol_tool_path)/templates/TypeBuilder_h.template',
      '<(protocol_tool_path)/CodeGenerator.py',
    ]
  },
  'defines': [
    'HAVE_INSPECTOR=1',
  ],
  'sources': [
    '../inspector_agent.cc',
    '../inspector_io.cc',
    '../inspector_agent.h',
    '../inspector_io.h',
    '../inspector_js_api.cc',
    '../inspector_socket.cc',
    '../inspector_socket.h',
    '../inspector_socket_server.cc',
    '../inspector_socket_server.h',
    'main_thread_interface.cc',
    'main_thread_interface.h',
    'node_string.cc',
    'node_string.h',
    'tracing_agent.cc',
    'tracing_agent.h',
    'worker_agent.cc',
    'worker_agent.h',
    'worker_inspector.cc',
    'worker_inspector.h',
  ],
  'include_dirs': [
    '<(node_inspector_generated_path)',
  ],
  'copies': [
    {
      'files': [
        '<(node_inspector_path)/node_protocol_config.json',
        '<(node_inspector_path)/node_protocol.pdl',
      ],
      'destination': '<(node_inspector_generated_path)',
    }
  ],
  'actions': [
    {
      'action_name': 'convert_node_protocol_to_json',
      'inputs': [
        '<(node_inspector_generated_path)/node_protocol.pdl',
      ],
      'outputs': [
        '<(node_inspector_generated_path)/node_protocol.json',
      ],
      'action': [
        'python',
        'tools/inspector_protocol/ConvertProtocolToJSON.py',
        '<@(_inputs)',
        '<@(_outputs)',
      ],
    },
    {
      'action_name': 'node_protocol_generated_sources',
      'process_outputs_as_sources': 0,
      'inputs': [
        '<(node_inspector_generated_path)/node_protocol_config.json',
        '<(node_inspector_generated_path)/node_protocol.json',
        '<@(node_protocol_files)',
      ],
      'outputs': [
        '<@(node_inspector_generated_sources)',
      ],
      'process_outputs_as_sources': 1,
      'action': [
        'python',
        'tools/inspector_protocol/CodeGenerator.py',
        '--jinja_dir', '<@(protocol_tool_path)/..',
        '--output_base', '<(node_inspector_generated_path)',
        '--config', '<(node_inspector_generated_path)/node_protocol_config.json',
      ],
      'message': 'Generating node protocol sources from protocol json',
    },
    {
      'action_name': 'concatenate_protocols',
      'inputs': [
        '../../deps/v8/src/inspector/js_protocol.json',
        '<(node_inspector_generated_path)/node_protocol.json',
      ],
      'outputs': [
        '<(node_inspector_generated_path)/concatenated_protocol.json',
      ],
      'action': [
        'python',
        'tools/inspector_protocol/ConcatenateProtocols.py',
        '<@(_inputs)',
        '<@(_outputs)',
      ],
    },
    {
      'action_name': 'v8_inspector_compress_protocol_json',
      'process_outputs_as_sources': 0,
      'inputs': [
        '<(node_inspector_generated_path)/concatenated_protocol.json',
      ],
      'outputs': [
        '<(node_inspector_generated_path)/concatenated_protocol/v8_inspector_protocol_json.h',
      ],
      'process_outputs_as_sources': 1,
      'action': [
        'python',
        'tools/compress_json.py',
        '<@(_inputs)',
        '<@(_outputs)',
      ],
    },
  ],
}
