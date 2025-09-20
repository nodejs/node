{
  'target_defaults': {
    'defines': [ '_U_=' ]
  },
  'variables': {
    'ngtcp2_sources': [
      'ngtcp2/lib/ngtcp2_acktr.c',
      'ngtcp2/lib/ngtcp2_addr.c',
      'ngtcp2/lib/ngtcp2_balloc.c',
      'ngtcp2/lib/ngtcp2_bbr.c',
      'ngtcp2/lib/ngtcp2_buf.c',
      'ngtcp2/lib/ngtcp2_callbacks.c',
      'ngtcp2/lib/ngtcp2_cc.c',
      'ngtcp2/lib/ngtcp2_cid.c',
      'ngtcp2/lib/ngtcp2_conn.c',
      'ngtcp2/lib/ngtcp2_conv.c',
      'ngtcp2/lib/ngtcp2_crypto.c',
      'ngtcp2/lib/ngtcp2_dcidtr.c',
      'ngtcp2/lib/ngtcp2_err.c',
      'ngtcp2/lib/ngtcp2_frame_chain.c',
      'ngtcp2/lib/ngtcp2_gaptr.c',
      'ngtcp2/lib/ngtcp2_idtr.c',
      'ngtcp2/lib/ngtcp2_ksl.c',
      'ngtcp2/lib/ngtcp2_log.c',
      'ngtcp2/lib/ngtcp2_map.c',
      'ngtcp2/lib/ngtcp2_mem.c',
      'ngtcp2/lib/ngtcp2_objalloc.c',
      'ngtcp2/lib/ngtcp2_opl.c',
      'ngtcp2/lib/ngtcp2_path.c',
      'ngtcp2/lib/ngtcp2_pcg.c',
      'ngtcp2/lib/ngtcp2_pkt.c',
      'ngtcp2/lib/ngtcp2_pmtud.c',
      'ngtcp2/lib/ngtcp2_ppe.c',
      'ngtcp2/lib/ngtcp2_pq.c',
      'ngtcp2/lib/ngtcp2_pv.c',
      'ngtcp2/lib/ngtcp2_qlog.c',
      'ngtcp2/lib/ngtcp2_range.c',
      'ngtcp2/lib/ngtcp2_ratelim.c',
      'ngtcp2/lib/ngtcp2_ringbuf.c',
      'ngtcp2/lib/ngtcp2_rob.c',
      'ngtcp2/lib/ngtcp2_rst.c',
      'ngtcp2/lib/ngtcp2_rtb.c',
      'ngtcp2/lib/ngtcp2_settings.c',
      'ngtcp2/lib/ngtcp2_str.c',
      'ngtcp2/lib/ngtcp2_strm.c',
      'ngtcp2/lib/ngtcp2_transport_params.c',
      'ngtcp2/lib/ngtcp2_unreachable.c',
      'ngtcp2/lib/ngtcp2_vec.c',
      'ngtcp2/lib/ngtcp2_version.c',
      'ngtcp2/lib/ngtcp2_window_filter.c',
      'ngtcp2/crypto/shared.c'
    ],
    'ngtcp2_sources_ossl': [
      'ngtcp2/crypto/ossl/ossl.c'
    ],
    'ngtcp2_sources_boringssl': [
      'ngtcp2/crypto/boringssl/boringssl.c'
    ],
    'nghttp3_sources': [
      'nghttp3/lib/nghttp3_balloc.c',
      'nghttp3/lib/nghttp3_buf.c',
      'nghttp3/lib/nghttp3_callbacks.c',
      'nghttp3/lib/nghttp3_conn.c',
      'nghttp3/lib/nghttp3_conv.c',
      'nghttp3/lib/nghttp3_debug.c',
      'nghttp3/lib/nghttp3_err.c',
      'nghttp3/lib/nghttp3_frame.c',
      'nghttp3/lib/nghttp3_gaptr.c',
      'nghttp3/lib/nghttp3_http.c',
      'nghttp3/lib/nghttp3_idtr.c',
      'nghttp3/lib/nghttp3_ksl.c',
      'nghttp3/lib/nghttp3_map.c',
      'nghttp3/lib/nghttp3_mem.c',
      'nghttp3/lib/nghttp3_objalloc.c',
      'nghttp3/lib/nghttp3_opl.c',
      'nghttp3/lib/nghttp3_pq.c',
      'nghttp3/lib/nghttp3_qpack.c',
      'nghttp3/lib/nghttp3_qpack_huffman.c',
      'nghttp3/lib/nghttp3_qpack_huffman_data.c',
      'nghttp3/lib/nghttp3_range.c',
      'nghttp3/lib/nghttp3_rcbuf.c',
      'nghttp3/lib/nghttp3_ringbuf.c',
      'nghttp3/lib/nghttp3_settings.c',
      'nghttp3/lib/nghttp3_str.c',
      'nghttp3/lib/nghttp3_stream.c',
      'nghttp3/lib/nghttp3_tnode.c',
      'nghttp3/lib/nghttp3_unreachable.c',
      'nghttp3/lib/nghttp3_vec.c',
      'nghttp3/lib/nghttp3_version.c',
    ],
    'ngtcp2_test_server_sources': [
      'ngtcp2/examples/tls_server_session_ossl.cc',
      'ngtcp2/examples/tls_server_context_ossl.cc',
      'ngtcp2/examples/tls_session_base_ossl.cc',
      'ngtcp2/examples/util_openssl.cc',
      'ngtcp2/examples/http.cc',
      'ngtcp2/examples/server_base.cc',
      'ngtcp2/examples/shared.cc',
      'ngtcp2/examples/server.cc',
      'ngtcp2/examples/util.cc',
      'ngtcp2/examples/debug.cc',
      'ngtcp2/examples/siphash.cc',
      'ngtcp2/third-party/urlparse/urlparse.c',
      'ngtcp2/third-party/libev/ev.c',
    ],
    'ngtcp2_test_client_sources': [
      'ngtcp2/examples/tls_client_session_ossl.cc',
      'ngtcp2/examples/tls_client_context_ossl.cc',
      'ngtcp2/examples/tls_session_base_ossl.cc',
      'ngtcp2/examples/util_openssl.cc',
      'ngtcp2/examples/http.cc',
      'ngtcp2/examples/client_base.cc',
      'ngtcp2/examples/shared.cc',
      'ngtcp2/examples/client.cc',
      'ngtcp2/examples/util.cc',
      'ngtcp2/examples/debug.cc',
      'ngtcp2/examples/siphash.cc',
      'ngtcp2/third-party/urlparse/urlparse.c',
      'ngtcp2/third-party/libev/ev.c',
    ]
  },
  'targets': [
    {
      'target_name': 'ngtcp2',
      'type': 'static_library',
      'include_dirs': [
        '',
        'ngtcp2/lib/includes/',
        'ngtcp2/crypto/includes/',
        'ngtcp2/lib/',
        'ngtcp2/crypto/',
      ],
      'defines': [
        'BUILDING_NGTCP2',
        'NGTCP2_STATICLIB',
      ],
      'conditions': [
        ['node_shared_openssl=="false"', {
          'dependencies': [
            '../openssl/openssl.gyp:openssl'
          ]
        }],
        ['OS!="win"', {
          'defines': ['HAVE_UNISTD_H']
        }],
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
        ['OS=="linux" or OS=="android" or OS=="openharmony"', {
          'defines': [
            'HAVE_ARPA_INET_H',
            'HAVE_NETINET_IN_H',
          ],
        }],
        # TODO: Support Boringssl here also. ngtcp2 provides an adapter
        # for Boringssl. If we can detect that boringssl is being used
        # here then we can use that adapter and also set the
        # QUIC_NGTCP2_USE_BORINGSSL define (the guard in quic/guard.h
        # would need to be updated to check for this define).
        ['node_shared_openssl=="false" and openssl_version >= 0x3050001f', {
          'sources': [
            '<@(ngtcp2_sources_ossl)',
          ],
          'direct_dependent_settings': {
            'defines': [
              # Tells us that we are using the OpenSSL 3.5 adapter
              # that is provided by ngtcp2.
              'QUIC_NGTCP2_USE_OPENSSL_3_5',
            ],
          },
        }]
      ],
      'direct_dependent_settings': {
        'defines': [
          'NGTCP2_STATICLIB',
        ],
        'include_dirs': [
          '',
          'ngtcp2/lib/includes',
          'ngtcp2/crypto/includes',
          'ngtcp2/crypto',
        ]
      },
      'sources': [
        '<@(ngtcp2_sources)',
      ]
    },
    {
      'target_name': 'nghttp3',
      'type': 'static_library',
      'include_dirs': [
        'nghttp3/lib/includes/',
        'nghttp3/lib/'
      ],
      'defines': [
        'BUILDING_NGHTTP3',
        'NGHTTP3_STATICLIB',
      ],
      'dependencies': [
        'ngtcp2'
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
        ['OS!="win"', {
          'defines': ['HAVE_UNISTD_H']
        }],
        ['OS=="linux" or OS=="android" or OS=="openharmony"', {
          'defines': [
            'HAVE_ARPA_INET_H',
            'HAVE_NETINET_IN_H',
          ],
        }],
      ],
      'direct_dependent_settings': {
        'defines': [
          'NGHTTP3_STATICLIB'
        ],
        'include_dirs': [
          'nghttp3/lib/includes'
        ]
      },
      'sources': [
        '<@(nghttp3_sources)'
      ]
    },
    {
      'target_name': 'ngtcp2_test_server',
      'type': 'executable',
      'cflags': [ '-Wno-everything' ],
      'include_dirs': [
        '',
        'ngtcp2/examples/',
        'ngtcp2/lib/includes/',
        'ngtcp2/crypto/includes/',
        'ngtcp2/third-party/urlparse/',
        'ngtcp2/third-party/libev/',
        '../../nghttp2/lib',
      ],
      'dependencies': [
        'ngtcp2',
        'nghttp3',
        '../openssl/openssl.gyp:openssl',
        '../nghttp2/nghttp2.gyp:sfparse',
      ],
      'defines': [
        'HAVE_CONFIG_H',
        'WITH_EXAMPLE_OSSL',
        'EV_STANDALONE=1',
        'HAVE_UNISTD_H',
        'HAVE_ARPA_INET_H',
        'HAVE_NETINET_IN_H',
        'HAVE_NETINET_IP_H',
      ],
      'conditions': [
        ['OS=="aix" or OS=="win"', {
          # AIX does not support some of the networking features used in
          # the test server. Windows also lacks the Unix-specific headers
          # and system calls required by the ngtcp2 examples.
          'type': 'none',  # Disable as executable on AIX and Windows
        }],
        ['OS=="mac"', {
          'defines': [
            '__APPLE_USE_RFC_3542',
          ]
        }],
        ['OS=="solaris"', {
          'defines': [
            'IPTOS_ECN_MASK=0x03',
          ],
          'link_settings': {
            'libraries': [ '-lsocket', '-lnsl' ],
          },
        }],
        [ 'OS=="linux" or OS=="openharmony"', {
          'link_settings': {
            'libraries': [ '-ldl', '-lrt' ],
          },
        }],
      ],
      'sources': [
        '<@(ngtcp2_test_server_sources)'
      ]
    },
    {
      'target_name': 'ngtcp2_test_client',
      'type': 'executable',
      'cflags': [ '-Wno-everything' ],
      'include_dirs': [
        '',
        'ngtcp2/examples/',
        'ngtcp2/lib/includes/',
        'ngtcp2/crypto/includes/',
        'ngtcp2/third-party/urlparse/',
        'ngtcp2/third-party/libev/',
        '../../nghttp2/lib',
      ],
      'dependencies': [
        'ngtcp2',
        'nghttp3',
        '../openssl/openssl.gyp:openssl',
        '../nghttp2/nghttp2.gyp:sfparse',
      ],
      'defines': [
        'HAVE_CONFIG_H',
        'WITH_EXAMPLE_OSSL',
        'EV_STANDALONE=1',
        'HAVE_UNISTD_H',
        'HAVE_ARPA_INET_H',
        'HAVE_NETINET_IN_H',
        'HAVE_NETINET_IP_H',
      ],
      'conditions': [
        ['OS=="aix" or OS=="win"', {
          # AIX does not support some of the networking features used in
          # the test client. Windows also lacks the Unix-specific headers
          # and system calls required by the ngtcp2 examples.
          'type': 'none',  # Disable as executable on AIX and Windows
        }],
        ['OS=="mac"', {
          'defines': [
            '__APPLE_USE_RFC_3542',
          ]
        }],
        ['OS=="solaris"', {
          'defines': [
            'IPTOS_ECN_MASK=0x03',
          ],
          'link_settings': {
            'libraries': [ '-lsocket', '-lnsl' ],
          },
        }],
        [ 'OS=="linux" or OS=="openharmony"', {
          'link_settings': {
            'libraries': [ '-ldl', '-lrt' ],
          },
        }],
      ],
      'sources': [
        '<@(ngtcp2_test_client_sources)'
      ]
    }
  ]
}
