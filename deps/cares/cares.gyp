{
  'target_defaults': {
    'conditions': [
      ['OS!="win"', {
        'defines': [
          '_DARWIN_USE_64_BIT_INODE=1',
          '_LARGEFILE_SOURCE',
          '_FILE_OFFSET_BITS=64',
          '_GNU_SOURCE'
        ]
      }],
      [ 'OS=="aix"', {
        'include_dirs': [ 'config/aix' ],
        'sources': [ 'config/aix/ares_config.h' ],
        'defines': [
          # Support for malloc(0)
          '_LINUX_SOURCE_COMPAT=1',
          '_ALL_SOURCE=1'],
      }],
      ['OS=="solaris"', {
        'defines': [
          '__EXTENSIONS__',
          '_XOPEN_SOURCE=500'
        ]
      }]
    ]
  },

  'targets': [
    {
      'target_name': 'cares',
      'type': '<(library)',
      'include_dirs': [ 'include', 'src' ],
      'direct_dependent_settings': {
        'include_dirs': [ 'include' ]
      },
      'sources': [
        'common.gypi',
        'include/ares.h',
        'include/ares_rules.h',
        'include/ares_version.h',
        'include/nameser.h',
        'src/ares_cancel.c',
        'src/ares__close_sockets.c',
        'src/ares_create_query.c',
        'src/ares_data.c',
        'src/ares_data.h',
        'src/ares_destroy.c',
        'src/ares_dns.h',
        'src/ares_expand_name.c',
        'src/ares_expand_string.c',
        'src/ares_fds.c',
        'src/ares_free_hostent.c',
        'src/ares_free_string.c',
        'src/ares_getenv.h',
        'src/ares_gethostbyaddr.c',
        'src/ares_gethostbyname.c',
        'src/ares__get_hostent.c',
        'src/ares_getnameinfo.c',
        'src/ares_getopt.c',
        'src/ares_getopt.h',
        'src/ares_getsock.c',
        'src/ares_init.c',
        'src/ares_ipv6.h',
        'src/ares_library_init.c',
        'src/ares_library_init.h',
        'src/ares_llist.c',
        'src/ares_llist.h',
        'src/ares_mkquery.c',
        'src/ares_nowarn.c',
        'src/ares_nowarn.h',
        'src/ares_options.c',
        'src/ares_parse_aaaa_reply.c',
        'src/ares_parse_a_reply.c',
        'src/ares_parse_mx_reply.c',
        'src/ares_parse_naptr_reply.c',
        'src/ares_parse_ns_reply.c',
        'src/ares_parse_ptr_reply.c',
        'src/ares_parse_soa_reply.c',
        'src/ares_parse_srv_reply.c',
        'src/ares_parse_txt_reply.c',
        'src/ares_platform.h',
        'src/ares_private.h',
        'src/ares_process.c',
        'src/ares_query.c',
        'src/ares__read_line.c',
        'src/ares_search.c',
        'src/ares_send.c',
        'src/ares_setup.h',
        'src/ares_strcasecmp.c',
        'src/ares_strcasecmp.h',
        'src/ares_strdup.c',
        'src/ares_strdup.h',
        'src/ares_strerror.c',
        'src/ares_timeout.c',
        'src/ares__timeval.c',
        'src/ares_version.c',
        'src/ares_writev.c',
        'src/ares_writev.h',
        'src/bitncmp.c',
        'src/bitncmp.h',
        'src/inet_net_pton.c',
        'src/inet_ntop.c',
        'src/ares_inet_net_pton.h',
        'src/setup_once.h',
      ],
      'conditions': [
        [ 'library=="static_library"', {
          'defines': [ 'CARES_STATICLIB' ]
        }, {
          'defines': [ 'CARES_BUILDING_LIBRARY' ]
        }],
        [ 'OS=="win"', {
          'defines': [ 'CARES_PULL_WS2TCPIP_H=1' ],
          'include_dirs': [ 'config/win32' ],
          'sources': [
            'src/config-win32.h',
            'src/windows_port.c',
            'src/ares_getenv.c',
            'src/ares_iphlpapi.h',
            'src/ares_platform.c'
          ],
          'libraries': [
            '-lws2_32.lib',
            '-liphlpapi.lib'
          ],
        }, {
          # Not Windows i.e. POSIX
          'cflags': [
            '-g',
            '-pedantic',
            '-Wall',
            '-Wextra',
            '-Wno-unused-parameter'
          ],
          'defines': [ 'HAVE_CONFIG_H' ],
        }],
        [ 'OS not in "win android"', {
          'cflags': [
            '--std=gnu89'
          ],
        }],
        [ 'OS=="linux"', {
          'include_dirs': [ 'config/linux' ],
          'sources': [ 'config/linux/ares_config.h' ]
        }],
        [ 'OS=="mac"', {
          'include_dirs': [ 'config/darwin' ],
          'sources': [ 'config/darwin/ares_config.h' ]
        }],
        [ 'OS=="freebsd" or OS=="dragonflybsd"', {
          'include_dirs': [ 'config/freebsd' ],
          'sources': [ 'config/freebsd/ares_config.h' ]
        }],
        [ 'OS=="openbsd"', {
          'include_dirs': [ 'config/openbsd' ],
          'sources': [ 'config/openbsd/ares_config.h' ]
        }],
        [ 'OS=="android"', {
          'include_dirs': [ 'config/android' ],
          'sources': [ 'config/android/ares_config.h' ],
        }],
        [ 'OS=="solaris"', {
          'include_dirs': [ 'config/sunos' ],
          'sources': [ 'config/sunos/ares_config.h' ],
          'direct_dependent_settings': {
            'libraries': [
              '-lsocket',
              '-lnsl'
            ]
          }
        }]
      ]
    }
  ]
}
