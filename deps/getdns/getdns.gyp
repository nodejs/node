{
  'targets': [
    {
      'target_name': 'getdns',
      'type': 'static_library',
      'include_dirs': [
        'src',
        '<(INTERMEDIATE_DIR)',
        'src/util/auxiliary',
        'src/openssl',
        'src/tls',
        'src/yxml',
      ],
      'libraries': ['-lunbound'],
      'direct_dependent_settings': {
        'include_dirs': [ 'src/getdns' ]
      },
      'actions': [
        {
          'action_name': 'configure_file',
          'process_outputs_as_sources': 1,
          'inputs': [
            # Put the code first so it's a dependency and can be used for invocation.
            'configure_file.py',
            'cmake/include/cmakeconfig.h.in',
            'src/getdns/getdns.h.in',
            'src/getdns/getdns_extra.h.in',
            'src/version.c.in',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/config.h',
            '<(INTERMEDIATE_DIR)/getdns.h',
            '<(INTERMEDIATE_DIR)/getdns_extra.h',
            '<(INTERMEDIATE_DIR)/version.c',
            '<(INTERMEDIATE_DIR)/getdns.pc',
          ],
          'action': [
            'python', '<@(_inputs)',
            '--target', '<@(_outputs)',
          ],
        },
      ],
      'sources': [
        'src/anchor.c',
        'src/const-info.c',
        'src/convert.c',
        'src/context.c',
        'src/dict.c',
        'src/dnssec.c',
        'src/general.c',
        'src/list.c',
        'src/request-internal.c',
        'src/mdns.c',
        'src/platform.c',
        'src/pubkey-pinning.c',
        'src/rr-dict.c',
        'src/rr-iter.c',
        'src/server.c',
        'src/stub.c',
        'src/sync.c',
        'src/ub_loop.c',
        'src/util-internal.c',

        'src/extension/select_eventloop.c',
        'src/extension/libuv.c',

        'src/gldns/keyraw.c',
        'src/gldns/gbuffer.c',
        'src/gldns/wire2str.c',
        'src/gldns/parse.c',
        'src/gldns/parseutil.c',
        'src/gldns/rrdef.c',
        'src/gldns/str2wire.c',

        'src/util/rbtree.c',
        'src/util/lruhash.c',
        'src/util/lookup3.c',
        'src/util/locks.c',

        'src/jsmn/jsmn.c',

        'src/yxml/yxml.c',

        'src/tls/val_secalgo.c',
        'src/tls/anchor-internal.c',

        'src/openssl/tls.c',
        'src/openssl/pubkey-pinning-internal.c',
        'src/openssl/keyraw-internal.c',


        'src/compat/arc4random.c',
        'src/compat/explicit_bzero.c',
        'src/compat/arc4_lock.c',
        'src/compat/arc4random_uniform.c',
        'src/compat/strlcpy.c',
      ]
    }
  ]
}
