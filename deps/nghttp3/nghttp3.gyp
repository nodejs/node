{
  'target_defaults': {
    'defines': [
      '_U_='
    ]
  },
  'targets': [
    {
      'target_name': 'nghttp3',
      'type': 'static_library',
      'include_dirs': ['lib/includes'],
      'defines': [
        'BUILDING_NGHTTP3',
        'NGHTTP3_STATICLIB',
      ],
      'conditions': [
        ['OS=="win"', {
          'defines': [
            'WIN32',
            '_WINDOWS',
            'HAVE_CONFIG_H',
          ],
          'msvs_settings': {
            'VCCLCompilerTool': {
              'CompileAs': '1'
            },
          },
        }],
        ['OS=="linux"', {
          'defines': [
            'HAVE_ARPA_INET_H',
          ],
        }],
      ],
      'direct_dependent_settings': {
        'defines': [ 'NGHTTP3_STATICLIB' ],
        'include_dirs': [ 'lib/includes' ]
      },
      'sources': [
        'lib/nghttp3_buf.c',
        'lib/nghttp3_conv.c',
        'lib/nghttp3_err.c',
        'lib/nghttp3_gaptr.c',
        'lib/nghttp3_idtr.c',
        'lib/nghttp3_map.c',
        'lib/nghttp3_pq.c',
        'lib/nghttp3_qpack_huffman.c',
        'lib/nghttp3_range.c',
        'lib/nghttp3_ringbuf.c',
        'lib/nghttp3_stream.c',
        'lib/nghttp3_vec.c',
        'lib/nghttp3_conn.c',
        'lib/nghttp3_debug.c',
        'lib/nghttp3_frame.c',
        'lib/nghttp3_http.c',
        'lib/nghttp3_ksl.c',
        'lib/nghttp3_mem.c',
        'lib/nghttp3_qpack.c',
        'lib/nghttp3_qpack_huffman_data.c',
        'lib/nghttp3_rcbuf.c',
        'lib/nghttp3_str.c',
        'lib/nghttp3_tnode.c',
        'lib/nghttp3_version.c'
      ]
    }
  ]
}
