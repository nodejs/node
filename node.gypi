{
  # 'force_load' means to include the static libs into the shared lib or
  # executable. Therefore, it is enabled when building:
  # 1. The executable and it uses static lib (cctest and node)
  # 2. The shared lib
  # Linker optimizes out functions that are not used. When force_load=true,
  # --whole-archive,force_load and /WHOLEARCHIVE are used to include
  # all obj files in static libs into the executable or shared lib.
  'variables': {
    'variables': {
      'variables': {
        'force_load%': 'true',
        'current_type%': '<(_type)',
      },
      'force_load%': '<(force_load)',
      'conditions': [
        ['current_type=="static_library"', {
          'force_load': 'false',
        }],
        [ 'current_type=="executable" and node_target_type=="shared_library"', {
          'force_load': 'false',
        }]
      ],
    },
    'force_load%': '<(force_load)',
  },
  # Putting these explicitly here so not to be dependant on common.gypi.
  'cflags': [ '-Wall', '-Wextra', '-Wno-unused-parameter', ],
  'xcode_settings': {
    'WARNING_CFLAGS': [
      '-Wall',
      '-Wendif-labels',
      '-W',
      '-Wno-unused-parameter',
      '-Werror=undefined-inline',
    ],
  },
  'conditions': [
    ['clang==1', {
      'cflags': [ '-Werror=undefined-inline', ]
    }],
    [ 'node_shared=="false"', {
      'msvs_settings': {
        'VCManifestTool': {
          'EmbedManifest': 'true',
          'AdditionalManifestFiles': 'src/res/node.exe.extra.manifest'
        }
      },
    }, {
      'defines': [
        'NODE_SHARED_MODE',
      ],
    }],
    [ 'OS=="win"', {
      'defines!': [
        'NODE_PLATFORM="win"',
      ],
      'defines': [
        'FD_SETSIZE=1024',
        # we need to use node's preferred "win32" rather than gyp's preferred "win"
        'NODE_PLATFORM="win32"',
        # Stop <windows.h> from defining macros that conflict with
        # std::min() and std::max().  We don't use <windows.h> (much)
        # but we still inherit it from uv.h.
        'NOMINMAX',
        '_UNICODE=1',
      ],
    }, { # POSIX
      'defines': [ '__POSIX__' ],
    }],

    [ 'node_enable_d8=="true"', {
      'dependencies': [ 'deps/v8/gypfiles/d8.gyp:d8' ],
    }],
    [ 'node_use_bundled_v8=="true"', {
      'conditions': [
        [ 'build_v8_with_gn=="true"', {
          'dependencies': ['deps/v8/gypfiles/v8-monolithic.gyp:v8_monolith'],
        }, {
          'dependencies': [
            'deps/v8/gypfiles/v8.gyp:v8',
            'deps/v8/gypfiles/v8.gyp:v8_libplatform',
          ],
        }],
      ],
    }],
    [ 'node_use_v8_platform=="true"', {
      'defines': [
        'NODE_USE_V8_PLATFORM=1',
      ],
    }, {
      'defines': [
        'NODE_USE_V8_PLATFORM=0',
      ],
    }],
    [ 'node_tag!=""', {
      'defines': [ 'NODE_TAG="<(node_tag)"' ],
    }],
    [ 'node_v8_options!=""', {
      'defines': [ 'NODE_V8_OPTIONS="<(node_v8_options)"'],
    }],
    [ 'node_release_urlbase!=""', {
      'defines': [
        'NODE_RELEASE_URLBASE="<(node_release_urlbase)"',
      ]
    }],
    [ 'v8_enable_i18n_support==1', {
      'defines': [ 'NODE_HAVE_I18N_SUPPORT=1' ],
      'dependencies': [
        '<(icu_gyp_path):icui18n',
        '<(icu_gyp_path):icuuc',
      ],
      'conditions': [
        [ 'icu_small=="true"', {
          'defines': [ 'NODE_HAVE_SMALL_ICU=1' ],
      }]],
    }],
    [ 'node_use_bundled_v8=="true" and \
       node_enable_v8_vtunejit=="true" and (target_arch=="x64" or \
       target_arch=="ia32" or target_arch=="x32")', {
      'defines': [ 'NODE_ENABLE_VTUNE_PROFILING' ],
      'dependencies': [
        'deps/v8/gypfiles/v8vtune.gyp:v8_vtune'
      ],
    }],
    [ 'node_no_browser_globals=="true"', {
      'defines': [ 'NODE_NO_BROWSER_GLOBALS' ],
    } ],
    [ 'node_use_bundled_v8=="true" and v8_postmortem_support=="true" and force_load=="true"', {
      'xcode_settings': {
        'OTHER_LDFLAGS': [
          '-Wl,-force_load,<(v8_base)',
        ],
      },
    }],
    [ 'node_shared_zlib=="false"', {
      'dependencies': [ 'deps/zlib/zlib.gyp:zlib' ],
      'conditions': [
        [ 'force_load=="true"', {
          'xcode_settings': {
            'OTHER_LDFLAGS': [
              '-Wl,-force_load,<(PRODUCT_DIR)/<(STATIC_LIB_PREFIX)'
                  'zlib<(STATIC_LIB_SUFFIX)',
            ],
          },
          'msvs_settings': {
            'VCLinkerTool': {
              'AdditionalOptions': [
                '/WHOLEARCHIVE:zlib<(STATIC_LIB_SUFFIX)',
              ],
            },
          },
          'conditions': [
            ['OS!="aix" and node_shared=="false"', {
              'ldflags': [
                '-Wl,--whole-archive,<(obj_dir)/deps/zlib/<(STATIC_LIB_PREFIX)'
                    'zlib<(STATIC_LIB_SUFFIX)',
                '-Wl,--no-whole-archive',
              ],
            }],
          ],
        }],
      ],
    }],

    [ 'node_shared_http_parser=="false"', {
      'dependencies': [
        'deps/http_parser/http_parser.gyp:http_parser',
        'deps/llhttp/llhttp.gyp:llhttp'
      ],
    } ],

    [ 'node_shared_cares=="false"', {
      'dependencies': [ 'deps/cares/cares.gyp:cares' ],
    }],

    [ 'node_shared_libuv=="false"', {
      'dependencies': [ 'deps/uv/uv.gyp:libuv' ],
      'conditions': [
        [ 'force_load=="true"', {
          'xcode_settings': {
            'OTHER_LDFLAGS': [
              '-Wl,-force_load,<(PRODUCT_DIR)/<(STATIC_LIB_PREFIX)'
                  'uv<(STATIC_LIB_SUFFIX)',
            ],
          },
          'msvs_settings': {
            'VCLinkerTool': {
              'AdditionalOptions': [
                '/WHOLEARCHIVE:libuv<(STATIC_LIB_SUFFIX)',
              ],
            },
          },
          'conditions': [
            ['OS!="aix" and node_shared=="false"', {
              'ldflags': [
                '-Wl,--whole-archive,<(obj_dir)/deps/uv/<(STATIC_LIB_PREFIX)'
                    'uv<(STATIC_LIB_SUFFIX)',
                '-Wl,--no-whole-archive',
              ],
            }],
          ],
        }],
      ],
    }],

    [ 'node_shared_nghttp2=="false"', {
      'dependencies': [ 'deps/nghttp2/nghttp2.gyp:nghttp2' ],
    }],

    [ 'node_shared_brotli=="false"', {
      'dependencies': [ 'deps/brotli/brotli.gyp:brotli' ],
    }],

    [ 'OS=="mac"', {
      # linking Corefoundation is needed since certain OSX debugging tools
      # like Instruments require it for some features
      'libraries': [ '-framework CoreFoundation' ],
      'defines!': [
        'NODE_PLATFORM="mac"',
      ],
      'defines': [
        # we need to use node's preferred "darwin" rather than gyp's preferred "mac"
        'NODE_PLATFORM="darwin"',
      ],
    }],
    [ 'OS=="freebsd"', {
      'libraries': [
        '-lutil',
        '-lkvm',
      ],
    }],
    [ 'OS=="aix"', {
      'defines': [
        '_LINUX_SOURCE_COMPAT',
        '__STDC_FORMAT_MACROS'
      ],
      'conditions': [
        [ 'force_load=="true"', {

          'actions': [
            {
              'action_name': 'expfile',
              'inputs': [
                '<(obj_dir)'
              ],
              'outputs': [
                '<(PRODUCT_DIR)/node.exp'
              ],
              'action': [
                'sh', 'tools/create_expfile.sh',
                      '<@(_inputs)', '<@(_outputs)'
              ],
            }
          ],
          'ldflags': ['-Wl,-bE:<(PRODUCT_DIR)/node.exp', '-Wl,-brtl'],
        }],
      ],
    }],
    [ 'OS=="solaris"', {
      'libraries': [
        '-lkstat',
        '-lumem',
      ],
      'defines!': [
        'NODE_PLATFORM="solaris"',
      ],
      'defines': [
        # we need to use node's preferred "sunos"
        # rather than gyp's preferred "solaris"
        'NODE_PLATFORM="sunos"',
      ],
    }],
    [ '(OS=="freebsd" or OS=="linux") and node_shared=="false"'
        ' and force_load=="true"', {
      'ldflags': [ '-Wl,-z,noexecstack',
                   '-Wl,--whole-archive <(v8_base)',
                   '-Wl,--no-whole-archive' ]
    }],
    [ 'coverage=="true" and node_shared=="false" and OS in "mac freebsd linux"', {
      'cflags!': [ '-O3' ],
      'ldflags': [ '--coverage',
                   '-g',
                   '-O0' ],
      'cflags': [ '--coverage',
                   '-g',
                   '-O0' ],
      'xcode_settings': {
        'OTHER_CFLAGS': [
          '--coverage',
          '-g',
          '-O0'
        ],
      },
      'conditions': [
        [ '_type=="executable"', {
          'xcode_settings': {
            'OTHER_LDFLAGS': [ '--coverage', ],
          },
        }],
      ],
    }],
    [ 'OS=="sunos"', {
      'ldflags': [ '-Wl,-M,/usr/lib/ld/map.noexstk' ],
    }],
    [ 'OS in "freebsd linux"', {
      'ldflags': [ '-Wl,-z,relro',
                   '-Wl,-z,now' ]
    }],
    [ 'OS=="linux" and target_arch=="x64" and node_use_large_pages=="true"', {
      'ldflags': [
        '-Wl,-T',
        '<!(realpath src/large_pages/ld.implicit.script)',
      ]
    }],
    [ 'node_use_openssl=="true"', {
      'defines': [ 'HAVE_OPENSSL=1' ],
      'conditions': [
        ['openssl_fips != "" or openssl_is_fips=="true"', {
          'defines': [ 'NODE_FIPS_MODE' ],
        }],
        [ 'node_shared_openssl=="false"', {
          'dependencies': [
            './deps/openssl/openssl.gyp:openssl',

            # For tests
            './deps/openssl/openssl.gyp:openssl-cli',
          ],
          'conditions': [
            # -force_load or --whole-archive are not applicable for
            # the static library
            [ 'force_load=="true"', {
              'xcode_settings': {
                'OTHER_LDFLAGS': [
                  '-Wl,-force_load,<(PRODUCT_DIR)/<(openssl_product)',
                ],
              },
              'msvs_settings': {
                'VCLinkerTool': {
                  'AdditionalOptions': [
                    '/WHOLEARCHIVE:<(openssl_product)',
                  ],
                },
              },
              'conditions': [
                ['OS in "linux freebsd" and node_shared=="false"', {
                  'ldflags': [
                    '-Wl,--whole-archive,'
                      '<(obj_dir)/deps/openssl/<(openssl_product)',
                    '-Wl,--no-whole-archive',
                  ],
                }],
                # openssl.def is based on zlib.def, zlib symbols
                # are always exported.
                ['use_openssl_def==1', {
                  'sources': ['<(SHARED_INTERMEDIATE_DIR)/openssl.def'],
                }],
                ['OS=="win" and use_openssl_def==0', {
                  'sources': ['deps/zlib/win32/zlib.def'],
                }],
              ],
            }],
          ],
        }]]

    }, {
      'defines': [ 'HAVE_OPENSSL=0' ]
    }],

  ],
}
