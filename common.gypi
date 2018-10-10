{
  'variables': {
    'asan%': 0,
    'werror': '',                     # Turn off -Werror in V8 build.
    'visibility%': 'hidden',          # V8's visibility setting
    'target_arch%': 'ia32',           # set v8's target architecture
    'host_arch%': 'ia32',             # set v8's host architecture
    'want_separate_host_toolset%': 0, # V8 should not build target and host
    'library%': 'static_library',     # allow override to 'shared_library' for DLL/.so builds
    'component%': 'static_library',   # NB. these names match with what V8 expects
    'msvs_multi_core_compile': '0',   # we do enable multicore compiles, but not using the V8 way
    'enable_pgo_generate%': '0',
    'enable_pgo_use%': '0',
    'python%': 'python',

    'node_shared%': 'false',
    'force_dynamic_crt%': 0,
    'node_use_v8_platform%': 'true',
    'node_use_bundled_v8%': 'true',
    'node_module_version%': '',
    'node_with_ltcg%': '',
    'node_use_pch%': 'false',
    'node_shared_openssl%': 'false',

    'node_tag%': '',
    'uv_library%': 'static_library',

    'clang%': 0,

    'openssl_fips%': '',

    # Some STL containers (e.g. std::vector) do not preserve ABI compatibility
    # between debug and non-debug mode.
    'disable_glibcxx_debug': 1,

    # Don't use ICU data file (icudtl.dat) from V8, we use our own.
    'icu_use_data_file_flag%': 0,

    # Reset this number to 0 on major V8 upgrades.
    # Increment by one for each non-official patch applied to deps/v8.
    'v8_embedder_string': '-node.0',

    ##### V8 defaults for Node.js #####

    # Old time default, now explicitly stated.
    'v8_use_snapshot': 'true',

    # These are more relevant for V8 internal development.
    # Refs: https://github.com/nodejs/node/issues/23122
    # Refs: https://github.com/nodejs/node/issues/23167
    # Enable compiler warnings when using V8_DEPRECATED apis from V8 code.
    'v8_deprecation_warnings': 0,
    # Enable compiler warnings when using V8_DEPRECATE_SOON apis from V8 code.
    'v8_imminent_deprecation_warnings': 0,

    # Enable disassembler for `--print-code` v8 options
    'v8_enable_disassembler': 1,

    # Don't bake anything extra into the snapshot.
    'v8_use_external_startup_data': 0,

    # https://github.com/nodejs/node/pull/22920/files#r222779926
    'v8_enable_handle_zapping': 0,

    # Disable V8 untrusted code mitigations.
    # See https://github.com/v8/v8/wiki/Untrusted-code-mitigations
    'v8_untrusted_code_mitigations': 'false',

    # Still WIP in V8 7.1
    'v8_enable_pointer_compression': 'false',

    # Explicitly set to false to copy V8's default
    'v8_enable_31bit_smis_on_64bit_arch': 'false',

    # New in V8 7.1
    'v8_enable_embedded_builtins': 'true',

    # This is more of a V8 dev setting
    # https://github.com/nodejs/node/pull/22920/files#r222779926
    'v8_enable_fast_mksnapshot': 0,

    ##### end V8 defaults #####

    'conditions': [
      ['target_arch=="arm64"', {
        # Disabled pending https://github.com/nodejs/node/issues/23913.
        'openssl_no_asm%': 1,
      }, {
        'openssl_no_asm%': 0,
      }],
      ['GENERATOR=="ninja"', {
        'obj_dir': '<(PRODUCT_DIR)/obj',
        'conditions': [
          [ 'build_v8_with_gn=="true"', {
            'v8_base': '<(PRODUCT_DIR)/obj/deps/v8/gypfiles/v8_monolith.gen/gn/obj/libv8_monolith.a',
          }, {
            'v8_base': '<(PRODUCT_DIR)/obj/deps/v8/gypfiles/libv8_base.a',
          }],
        ]
       }, {
        'obj_dir%': '<(PRODUCT_DIR)/obj.target',
        'v8_base': '<(PRODUCT_DIR)/obj.target/deps/v8/gypfiles/libv8_base.a',
      }],
      ['OS == "win"', {
        'os_posix': 0,
        'v8_postmortem_support%': 'false',
        'obj_dir': '<(PRODUCT_DIR)/obj',
        'v8_base': '<(PRODUCT_DIR)/lib/v8_libbase.lib',
      }, {
        'os_posix': 1,
        'v8_postmortem_support%': 'true',
      }],
      ['OS == "mac"', {
        'obj_dir%': '<(PRODUCT_DIR)/obj.target',
        'v8_base': '<(PRODUCT_DIR)/libv8_base.a',
      }],
      ['build_v8_with_gn == "true"', {
        'conditions': [
          ['GENERATOR == "ninja"', {
            'v8_base': '<(PRODUCT_DIR)/obj/deps/v8/gypfiles/v8_monolith.gen/gn/obj/libv8_monolith.a',
          }, {
            'v8_base': '<(PRODUCT_DIR)/obj.target/v8_monolith/geni/gn/obj/libv8_monolith.a',
          }],
        ],
      }],
      ['openssl_fips != ""', {
        'openssl_product': '<(STATIC_LIB_PREFIX)crypto<(STATIC_LIB_SUFFIX)',
      }, {
        'openssl_product': '<(STATIC_LIB_PREFIX)openssl<(STATIC_LIB_SUFFIX)',
      }],
      ['OS=="mac"', {
        'clang%': 1,
      }],
    ],
  },

  'target_defaults': {
    'default_configuration': 'Release',
    'configurations': {
      'Debug': {
        'variables': {
          'v8_enable_handle_zapping': 1,
        },
        'defines': [ 'DEBUG', '_DEBUG', 'V8_ENABLE_CHECKS' ],
        'cflags': [ '-g', '-O0' ],
        'conditions': [
          ['target_arch=="x64"', {
            'msvs_configuration_platform': 'x64',
          }],
          ['OS=="aix"', {
            'cflags': [ '-gxcoff' ],
            'ldflags': [ '-Wl,-bbigtoc' ],
          }],
          ['OS == "android"', {
            'cflags': [ '-fPIE' ],
            'ldflags': [ '-fPIE', '-pie' ]
          }],
          ['node_shared=="true"', {
            'msvs_settings': {
             'VCCLCompilerTool': {
               'RuntimeLibrary': 3, # MultiThreadedDebugDLL (/MDd)
             }
            }
          }],
          ['node_shared=="false"', {
            'msvs_settings': {
              'VCCLCompilerTool': {
                'RuntimeLibrary': 1 # MultiThreadedDebug (/MTd)
              }
            }
          }]
        ],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'Optimization': 0, # /Od, no optimization
            'MinimalRebuild': 'false',
            'OmitFramePointers': 'false',
            'BasicRuntimeChecks': 3, # /RTC1
            'MultiProcessorCompilation': 'true',
            'AdditionalOptions': [
              '/bigobj', # prevent error C1128 in VS2015
            ],
          },
          'VCLinkerTool': {
            'LinkIncremental': 2, # enable incremental linking
          },
        },
        'xcode_settings': {
          'GCC_OPTIMIZATION_LEVEL': '0', # stop gyp from defaulting to -Os
        },
      },
      'Release': {
        'variables': {
          'v8_enable_handle_zapping': 0,
        },
        'cflags': [ '-O3' ],
        'conditions': [
          ['target_arch=="x64"', {
            'msvs_configuration_platform': 'x64',
          }],
          ['OS=="solaris"', {
            # pull in V8's postmortem metadata
            'ldflags': [ '-Wl,-z,allextract' ]
          }],
          ['OS!="mac" and OS!="win"', {
            'cflags': [ '-fno-omit-frame-pointer' ],
          }],
          ['OS=="linux"', {
            'variables': {
              'pgo_generate': ' -fprofile-generate ',
              'pgo_use': ' -fprofile-use -fprofile-correction ',
              'lto': ' -flto=4 -fuse-linker-plugin -ffat-lto-objects ',
            },
            'conditions': [
              ['enable_pgo_generate=="true"', {
                'cflags': ['<(pgo_generate)'],
                'ldflags': ['<(pgo_generate)'],
              },],
              ['enable_pgo_use=="true"', {
                'cflags': ['<(pgo_use)'],
                'ldflags': ['<(pgo_use)'],
              },],
              ['enable_lto=="true"', {
                'cflags': ['<(lto)'],
                'ldflags': ['<(lto)'],
              },],
            ],
          },],
          ['OS == "android"', {
            'cflags': [ '-fPIE' ],
            'ldflags': [ '-fPIE', '-pie' ]
          }],
          ['node_shared=="true"', {
            'msvs_settings': {
             'VCCLCompilerTool': {
               'RuntimeLibrary': 2 # MultiThreadedDLL (/MD)
             }
            }
          }],
          ['node_shared=="false"', {
            'msvs_settings': {
              'VCCLCompilerTool': {
                'RuntimeLibrary': 0 # MultiThreaded (/MT)
              }
            }
          }],
          ['node_with_ltcg=="true"', {
            'msvs_settings': {
              'VCCLCompilerTool': {
                'WholeProgramOptimization': 'true' # /GL, whole program optimization, needed for LTCG
              },
              'VCLibrarianTool': {
                'AdditionalOptions': [
                  '/LTCG:INCREMENTAL', # link time code generation
                ]
              },
              'VCLinkerTool': {
                'OptimizeReferences': 2, # /OPT:REF
                'EnableCOMDATFolding': 2, # /OPT:ICF
                'LinkIncremental': 1, # disable incremental linking
                'AdditionalOptions': [
                  '/LTCG:INCREMENTAL', # incremental link-time code generation
                ]
              }
            }
          }, {
            'msvs_settings': {
              'VCCLCompilerTool': {
                'WholeProgramOptimization': 'false'
              },
              'VCLinkerTool': {
                'LinkIncremental': 2 # enable incremental linking
              }
            }
          }]
        ],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'Optimization': 3, # /Ox, full optimization
            'FavorSizeOrSpeed': 1, # /Ot, favor speed over size
            'InlineFunctionExpansion': 2, # /Ob2, inline anything eligible
            'OmitFramePointers': 'true',
            'EnableFunctionLevelLinking': 'true',
            'EnableIntrinsicFunctions': 'true',
            'RuntimeTypeInfo': 'false',
            'MultiProcessorCompilation': 'true',
            'AdditionalOptions': [
            ],
          }
        }
      }
    },

    # Defines these mostly for node-gyp to pickup, and warn addon authors of
    # imminent V8 deprecations, also to sync how dependencies are configured.
    'defines': [
      'V8_DEPRECATION_WARNINGS',
      'V8_IMMINENT_DEPRECATION_WARNINGS',
    ],

    # Forcibly disable -Werror.  We support a wide range of compilers, it's
    # simply not feasible to squelch all warnings, never mind that the
    # libraries in deps/ are not under our control.
    'cflags!': ['-Werror'],
    'msvs_settings': {
      'VCCLCompilerTool': {
        'StringPooling': 'true', # pool string literals
        'DebugInformationFormat': 1, # /Z7 embed info in .obj files
        'WarningLevel': 3,
        'BufferSecurityCheck': 'true',
        'ExceptionHandling': 0, # /EHsc
        'SuppressStartupBanner': 'true',
        'WarnAsError': 'false',
      },
      'VCLinkerTool': {
        'conditions': [
          ['target_arch=="ia32"', {
            'TargetMachine' : 1, # /MACHINE:X86
            'target_conditions': [
              ['_type=="executable"', {
                'AdditionalOptions': [ '/SubSystem:Console,"5.01"' ],
              }],
            ],
          }],
          ['target_arch=="x64"', {
            'TargetMachine' : 17, # /MACHINE:AMD64
            'target_conditions': [
              ['_type=="executable"', {
                'AdditionalOptions': [ '/SubSystem:Console,"5.02"' ],
              }],
            ],
          }],
        ],
        'GenerateDebugInformation': 'true',
        'GenerateMapFile': 'true', # /MAP
        'MapExports': 'true', # /MAPINFO:EXPORTS
        'RandomizedBaseAddress': 2, # enable ASLR
        'DataExecutionPrevention': 2, # enable DEP
        'AllowIsolation': 'true',
        'SuppressStartupBanner': 'true',
      },
    },
    # Disable warnings:
    # - "C4251: class needs to have dll-interface"
    # - "C4275: non-DLL-interface used as base for DLL-interface"
    #   Over 10k of these warnings are generated when compiling node,
    #   originating from v8.h. Most of them are false positives.
    #   See also: https://github.com/nodejs/node/pull/15570
    #   TODO: re-enable when Visual Studio fixes these upstream.
    #
    # - "C4267: conversion from 'size_t' to 'int'"
    #   Many any originate from our dependencies, and their sheer number
    #   drowns out other, more legitimate warnings.
    # - "C4244: conversion from 'type1' to 'type2', possible loss of data"
    #   Ususaly safe. Disable for `dep`, enable for `src`
    'msvs_disabled_warnings': [4351, 4355, 4800, 4251, 4275, 4244, 4267],
    'conditions': [
      ['asan == 1 and OS != "mac"', {
        'cflags+': [
          '-fno-omit-frame-pointer',
          '-fsanitize=address',
          '-DLEAK_SANITIZER'
        ],
        'cflags!': [ '-fomit-frame-pointer' ],
        'ldflags': [ '-fsanitize=address' ],
      }],
      ['asan == 1 and OS == "mac"', {
        'xcode_settings': {
          'OTHER_CFLAGS+': [
            '-fno-omit-frame-pointer',
            '-gline-tables-only',
            '-fsanitize=address',
            '-DLEAK_SANITIZER'
          ],
          'OTHER_CFLAGS!': [
            '-fomit-frame-pointer',
          ],
        },
        'target_conditions': [
          ['_type!="static_library"', {
            'xcode_settings': {'OTHER_LDFLAGS': ['-fsanitize=address']},
          }],
        ],
      }],
      ['OS == "win"', {
        'msvs_cygwin_shell': 0, # prevent actions from trying to use cygwin
        'defines': [
          'WIN32',
          # we don't really want VC++ warning us about
          # how dangerous C functions are...
          '_CRT_SECURE_NO_DEPRECATE',
          # ... or that C implementations shouldn't use
          # POSIX names
          '_CRT_NONSTDC_NO_DEPRECATE',
          # Make sure the STL doesn't try to use exceptions
          '_HAS_EXCEPTIONS=0',
          'BUILDING_V8_SHARED=1',
          'BUILDING_UV_SHARED=1',
        ],
      }],
      [ 'OS in "linux freebsd openbsd solaris aix"', {
        'cflags': [ '-pthread' ],
        'ldflags': [ '-pthread' ],
      }],
      [ 'OS in "linux freebsd openbsd solaris android aix cloudabi"', {
        'cflags': [ '-Wall', '-Wextra', '-Wno-unused-parameter', ],
        'cflags_cc': [ '-fno-rtti', '-fno-exceptions', '-std=gnu++1y' ],
        'ldflags': [ '-rdynamic' ],
        'target_conditions': [
          # The 1990s toolchain on SmartOS can't handle thin archives.
          ['_type=="static_library" and OS=="solaris"', {
            'standalone_static_library': 1,
          }],
          ['OS=="openbsd"', {
            'cflags': [ '-I/usr/local/include' ],
            'ldflags': [ '-Wl,-z,wxneeded' ],
          }],
        ],
        'conditions': [
          [ 'target_arch=="ia32"', {
            'cflags': [ '-m32' ],
            'ldflags': [ '-m32' ],
          }],
          [ 'target_arch=="x32"', {
            'cflags': [ '-mx32' ],
            'ldflags': [ '-mx32' ],
          }],
          [ 'target_arch=="x64"', {
            'cflags': [ '-m64' ],
            'ldflags': [ '-m64' ],
          }],
          [ 'target_arch=="ppc" and OS!="aix"', {
            'cflags': [ '-m32' ],
            'ldflags': [ '-m32' ],
          }],
          [ 'target_arch=="ppc64" and OS!="aix"', {
            'cflags': [ '-m64', '-mminimal-toc' ],
            'ldflags': [ '-m64' ],
          }],
          [ 'target_arch=="s390"', {
            'cflags': [ '-m31', '-march=z196' ],
            'ldflags': [ '-m31', '-march=z196' ],
          }],
          [ 'target_arch=="s390x"', {
            'cflags': [ '-m64', '-march=z196' ],
            'ldflags': [ '-m64', '-march=z196' ],
          }],
          [ 'OS=="solaris"', {
            'cflags': [ '-pthreads' ],
            'ldflags': [ '-pthreads' ],
            'cflags!': [ '-pthread' ],
            'ldflags!': [ '-pthread' ],
          }],
          [ 'node_shared=="true"', {
            'cflags': [ '-fPIC' ],
          }],
        ],
      }],
      [ 'OS=="aix"', {
        'variables': {
          # Used to differentiate `AIX` and `OS400`(IBM i).
          'aix_variant_name': '<!(uname -s)',
        },
        'cflags': [ '-maix64', ],
        'ldflags!': [ '-rdynamic', ],
        'ldflags': [
          '-Wl,-bbigtoc',
          '-maix64',
        ],
        'conditions': [
          [ '"<(aix_variant_name)"=="OS400"', {            # a.k.a. `IBM i`
            'ldflags': [
              '-Wl,-blibpath:/QOpenSys/pkgs/lib:/QOpenSys/usr/lib',
              '-Wl,-brtl',
            ],
          }, {                                             # else it's `AIX`
            'ldflags': [
              '-Wl,-blibpath:/usr/lib:/lib:/opt/freeware/lib/pthread/ppc64',
            ],
          }],
        ],
      }],
      ['OS=="android"', {
        'target_conditions': [
          ['_toolset=="target"', {
            'defines': [ '_GLIBCXX_USE_C99_MATH' ],
            'libraries': [ '-llog' ],
          }],
        ],
      }],
      ['OS=="mac"', {
        'defines': ['_DARWIN_USE_64_BIT_INODE=1'],
        'xcode_settings': {
          'ALWAYS_SEARCH_USER_PATHS': 'NO',
          'GCC_CW_ASM_SYNTAX': 'NO',                # No -fasm-blocks
          'GCC_DYNAMIC_NO_PIC': 'NO',               # No -mdynamic-no-pic
                                                    # (Equivalent to -fPIC)
          'GCC_ENABLE_CPP_EXCEPTIONS': 'NO',        # -fno-exceptions
          'GCC_ENABLE_CPP_RTTI': 'NO',              # -fno-rtti
          'GCC_ENABLE_PASCAL_STRINGS': 'NO',        # No -mpascal-strings
          'PREBINDING': 'NO',                       # No -Wl,-prebind
          'MACOSX_DEPLOYMENT_TARGET': '10.7',       # -mmacosx-version-min=10.7
          'USE_HEADERMAP': 'NO',
          'OTHER_CFLAGS': [
            '-fno-strict-aliasing',
          ],
          'WARNING_CFLAGS': [
            '-Wall',
            '-Wendif-labels',
            '-W',
            '-Wno-unused-parameter',
          ],
        },
        'target_conditions': [
          ['_type!="static_library"', {
            'xcode_settings': {
              'OTHER_LDFLAGS': [
                '-Wl,-no_pie',
                '-Wl,-search_paths_first',
              ],
            },
          }],
        ],
        'conditions': [
          ['target_arch=="ia32"', {
            'xcode_settings': {'ARCHS': ['i386']},
          }],
          ['target_arch=="x64"', {
            'xcode_settings': {'ARCHS': ['x86_64']},
          }],
          ['clang==1', {
            'xcode_settings': {
              'GCC_VERSION': 'com.apple.compilers.llvm.clang.1_0',
              'CLANG_CXX_LANGUAGE_STANDARD': 'gnu++1y',  # -std=gnu++1y
              'CLANG_CXX_LIBRARY': 'libc++',
            },
          }],
        ],
      }],
      ['OS=="freebsd" and node_use_dtrace=="true"', {
        'libraries': [ '-lelf' ],
      }],
      ['OS=="freebsd"', {
        'ldflags': [
          '-Wl,--export-dynamic',
        ],
      }],
      ['node_shared_openssl!="true"', {
        # `OPENSSL_THREADS` is defined via GYP for openSSL for all architectures.
        'defines': [
          'OPENSSL_THREADS',
        ],
      }],
      ['node_shared_openssl!="true" and openssl_no_asm==1', {
        'defines': [
          'OPENSSL_NO_ASM',
        ],
      }],
    ],
  }
}
