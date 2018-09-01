{
  'variables': {
    'protocol_tool_path': '../../tools/inspector_protocol',
  },
  'targets': [
    {
      'target_name': 'generate_concatenated_inspector_protocol',
      'type': 'none',
      'inputs': [
        '../../deps/v8/src/inspector/js_protocol.pdl',
        'node_protocol.json',
      ],
      'outputs': [
        '<(SHARED_INTERMEDIATE_DIR)/v8_inspector_protocol_json.h',
      ],
      'actions': [
        {
          'action_name': 'v8_inspector_convert_protocol_to_json',
          'inputs': [
            '../../deps/v8/src/inspector/js_protocol.pdl',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/js_protocol.json',
          ],
          'action': [
            'python',
            '<(protocol_tool_path)/ConvertProtocolToJSON.py',
            '<@(_inputs)',
            '<@(_outputs)',
          ],
        },
        {
          'action_name': 'concatenate_protocols',
          'inputs': [
            '<(SHARED_INTERMEDIATE_DIR)/js_protocol.json',
            'node_protocol.json',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/concatenated_protocol.json',
          ],
          'action': [
            'python',
            '<(protocol_tool_path)/ConcatenateProtocols.py',
            '<@(_inputs)',
            '<@(_outputs)',
          ],
        },
        {
          'action_name': 'v8_inspector_compress_protocol_json',
          'process_outputs_as_sources': 1,
          'inputs': [
            '<(SHARED_INTERMEDIATE_DIR)/concatenated_protocol.json',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/v8_inspector_protocol_json.h',
          ],
          'action': [
            'python',
            '../../tools/compress_json.py',
            '<@(_inputs)',
            '<@(_outputs)',
          ],
        },
      ],
    },
  ]
}