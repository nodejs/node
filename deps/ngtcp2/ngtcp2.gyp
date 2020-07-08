{
  'target_defaults': {
    'defines': [
      '_U_='
    ]
  },
  'targets': [
    {
      'target_name': 'ngtcp2',
      'type': 'static_library',
      'include_dirs': [
        'lib/includes',
        'lib',
        'crypto/includes',
        'crypto'
      ],
      'defines': [
        'BUILDING_NGTCP2',
        'NGTCP2_STATICLIB',
      ],
      'dependencies': [
        '../openssl/openssl.gyp:openssl'
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
            'HAVE_NETINET_IN_H',
          ],
        }],
      ],
      'direct_dependent_settings': {
        'defines': [ 'NGTCP2_STATICLIB' ],
        'include_dirs': [
          'lib/includes',
          'crypto/includes',
        ]
      },
      'sources': [
        'lib/ngtcp2_acktr.c',
        'lib/ngtcp2_acktr.h',
        'lib/ngtcp2_addr.c',
        'lib/ngtcp2_addr.h',
        'lib/ngtcp2_buf.c',
        'lib/ngtcp2_buf.h',
        'lib/ngtcp2_cc.c',
        'lib/ngtcp2_cc.h',
        'lib/ngtcp2_cid.c',
        'lib/ngtcp2_cid.h',
        'lib/ngtcp2_conn.c',
        'lib/ngtcp2_conn.h',
        'lib/ngtcp2_conv.c',
        'lib/ngtcp2_conv.h',
        'lib/ngtcp2_crypto.c',
        'lib/ngtcp2_crypto.h',
        'lib/ngtcp2_err.c',
        'lib/ngtcp2_err.h',
        'lib/ngtcp2_gaptr.c',
        'lib/ngtcp2_gaptr.h',
        'lib/ngtcp2_idtr.c',
        'lib/ngtcp2_idtr.h',
        'lib/ngtcp2_ksl.c',
        'lib/ngtcp2_ksl.h',
        'lib/ngtcp2_log.c',
        'lib/ngtcp2_log.h',
        'lib/ngtcp2_macro.h',
        'lib/ngtcp2_map.c',
        'lib/ngtcp2_map.h',
        'lib/ngtcp2_mem.c',
        'lib/ngtcp2_mem.h',
        'lib/ngtcp2_path.c',
        'lib/ngtcp2_path.h',
        'lib/ngtcp2_pkt.c',
        'lib/ngtcp2_pkt.h',
        'lib/ngtcp2_ppe.c',
        'lib/ngtcp2_ppe.h',
        'lib/ngtcp2_pq.c',
        'lib/ngtcp2_pq.h',
        'lib/ngtcp2_pv.c',
        'lib/ngtcp2_pv.h',
        'lib/ngtcp2_qlog.c',
        'lib/ngtcp2_qlog.h',
        'lib/ngtcp2_range.c',
        'lib/ngtcp2_range.h',
        'lib/ngtcp2_ringbuf.c',
        'lib/ngtcp2_ringbuf.h',
        'lib/ngtcp2_rob.c',
        'lib/ngtcp2_rob.h',
        'lib/ngtcp2_rtb.c',
        'lib/ngtcp2_rtb.h',
        'lib/ngtcp2_rst.c',
        'lib/ngtcp2_rst.h',
        'lib/ngtcp2_str.c',
        'lib/ngtcp2_str.h',
        'lib/ngtcp2_strm.c',
        'lib/ngtcp2_strm.h',
        'lib/ngtcp2_vec.c',
        'lib/ngtcp2_vec.h',
        'lib/ngtcp2_version.c',
        'crypto/shared.c',
        'crypto/shared.h',
        'crypto/openssl/openssl.c'
      ]
    }
  ]
}
