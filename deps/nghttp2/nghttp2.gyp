{
  'target_defaults': {
    'defines': [
      '_U_='
    ]
  },
  'targets': [
    {
      'target_name': 'nghttp2',
      'type': 'static_library',
      'include_dirs': ['lib/includes'],
      'conditions': [
        ['OS=="win"', {
          'defines': [
            'WIN32',
            '_WINDOWS',
            'HAVE_CONFIG_H',
            'NGHTTP2_STATICLIB',
          ],
          'msvs_settings': {
            'VCCLCompilerTool': {
              'CompileAs': '1'
            },
          },
        }],
        ['debug_nghttp2 == 1', {
          'defines': [ 'DEBUGBUILD=1' ]
        }]
      ],
      'direct_dependent_settings': {
        'include_dirs': [ 'lib/includes' ]
      },
      'sources': [
        'lib/nghttp2_buf.c',
        'lib/nghttp2_callbacks.c',
        'lib/nghttp2_debug.c',
        'lib/nghttp2_frame.c',
        'lib/nghttp2_hd.c',
        'lib/nghttp2_hd_huffman.c',
        'lib/nghttp2_hd_huffman_data.c',
        'lib/nghttp2_helper.c',
        'lib/nghttp2_http.c',
        'lib/nghttp2_map.c',
        'lib/nghttp2_mem.c',
        'lib/nghttp2_npn.c',
        'lib/nghttp2_option.c',
        'lib/nghttp2_outbound_item.c',
        'lib/nghttp2_pq.c',
        'lib/nghttp2_priority_spec.c',
        'lib/nghttp2_queue.c',
        'lib/nghttp2_rcbuf.c',
        'lib/nghttp2_session.c',
        'lib/nghttp2_stream.c',
        'lib/nghttp2_submit.c',
        'lib/nghttp2_version.c'
      ]
    }
  ]
}
