{
  'targets': [
    {
      'target_name': 'getdns',
      'type': 'static_library',
      'include_dirs': [
        'getdns/src',
        'getdns/src/getdns',
        'getdns/src/util/auxiliary',
        'getdns/src/openssl',
        'getdns/src/tls',
        'getdns/src/yxml',
        '<(SHARED_INTERMEDIATE_DIR)/getdns/src',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'getdns/src',
          '<(SHARED_INTERMEDIATE_DIR)/getdns/src',
        ],
      },
      'actions': [
        {
          'action_name': 'configure_file',
          'process_outputs_as_sources': 1,
          'inputs': [
            # Put the code first so it's a dependency and can be used for invocation.
            'configure_file.py',
            'getdns/cmake/include/cmakeconfig.h.in',
            'getdns/src/getdns/getdns.h.in',
            'getdns/src/getdns/getdns_extra.h.in',
            'getdns/src/version.c.in',
            'getdns/getdns.pc.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/getdns/src/config.h',
            '<(SHARED_INTERMEDIATE_DIR)/getdns/src/getdns/getdns.h',
            '<(SHARED_INTERMEDIATE_DIR)/getdns/src/getdns/getdns_extra.h',
            '<(SHARED_INTERMEDIATE_DIR)/getdns/src/version.c',
            '<(SHARED_INTERMEDIATE_DIR)/getdns/getdns.pc',
          ],
          'action': [
            'python', '<@(_inputs)', '--target', '<@(_outputs)',
          ],
        },
      ],
      'conditions': [
        ['node_shared_openssl=="false"', {
          'dependencies': [
            '../openssl/openssl.gyp:openssl'
          ]
        }],
        ['node_shared_libuv=="false"', {
          'dependencies': [
            '../uv/uv.gyp:libuv'
          ]
        }],
        [ 'OS=="linux"', {
          'defines': [
            '_GNU_SOURCE',
            '_DEFAULT_SOURCE',
          ],
          'sources': [
            'getdns/src/compat/arc4random.c',
            'getdns/src/compat/explicit_bzero.c',
            'getdns/src/compat/arc4_lock.c',
            'getdns/src/compat/arc4random_uniform.c',
            'getdns/src/compat/strlcpy.c',
          ],
        }],
        [ 'OS=="win"', {
          'sources': [
            'getdns/src/compat/gettimeofday.c',
            'getdns/src/compat/arc4random.c',
            'getdns/src/compat/arc4_lock.c',
            'getdns/src/compat/arc4random_uniform.c',
            'getdns/src/compat/getentropy_win.c',
            'getdns/src/compat/strlcpy.c',
            'getdns/src/compat/mkstemp.c',
            'getdns/src/compat/strptime.c',
          ],
        }],
        [ 'OS=="freebsd"', {
          'defines': [
            '_POSIX_C_SOURCE=200112L',
            '_XOPEN_SOURCE=600',
          ],
        }],
        [ 'OS=="solaris"', {
          'defines': [
            '__EXTENSIONS_',
          ],
        }],
        [ 'OS in "mac ios"', {
          'defines': ['_DARWIN_C_SOURCE'],
        }],
      ],
      'sources': [
        'getdns/src/anchor.c',
        'getdns/src/const-info.c',
        'getdns/src/convert.c',
        'getdns/src/context.c',
        'getdns/src/dict.c',
        'getdns/src/dnssec.c',
        'getdns/src/general.c',
        'getdns/src/list.c',
        'getdns/src/request-internal.c',
        'getdns/src/mdns.c',
        'getdns/src/platform.c',
        'getdns/src/pubkey-pinning.c',
        'getdns/src/rr-dict.c',
        'getdns/src/rr-iter.c',
        'getdns/src/server.c',
        'getdns/src/stub.c',
        'getdns/src/sync.c',
        'getdns/src/ub_loop.c',
        'getdns/src/util-internal.c',

        'getdns/src/extension/select_eventloop.c',
        'getdns/src/extension/libuv.c',

        'getdns/src/gldns/keyraw.c',
        'getdns/src/gldns/gbuffer.c',
        'getdns/src/gldns/wire2str.c',
        'getdns/src/gldns/parse.c',
        'getdns/src/gldns/parseutil.c',
        'getdns/src/gldns/rrdef.c',
        'getdns/src/gldns/str2wire.c',

        'getdns/src/util/rbtree.c',
        'getdns/src/util/lruhash.c',
        'getdns/src/util/lookup3.c',
        'getdns/src/util/locks.c',

        'getdns/src/jsmn/jsmn.c',

        'getdns/src/yxml/yxml.c',

        'getdns/src/tls/val_secalgo.c',
        'getdns/src/tls/anchor-internal.c',

        'getdns/src/openssl/tls.c',
        'getdns/src/openssl/pubkey-pinning-internal.c',
        'getdns/src/openssl/keyraw-internal.c',

        '<(SHARED_INTERMEDIATE_DIR)/getdns/src/version.c',
      ]
    }
  ]
}
