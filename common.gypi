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
    'python%': 'python',

    'node_shared%': 'false',
    'force_dynamic_crt%': 0,
    'node_use_v8_platform%': 'true',
    'node_use_bundled_v8%': 'true',
    'node_module_version%': '',

    'node_tag%': '',
    'uv_library%': 'static_library',

    'openssl_fips%': '',

    # Default to -O0 for debug builds.
    'v8_optimized_debug%': 0,

    # Reset this number to 0 on major V8 upgrades.
    # Increment by one for each non-official patch applied to deps/v8.
    'v8_embedder_string': '-node.3',

    # Enable disassembler for `--print-code` v8 options
    'v8_enable_disassembler': 1,

    # Don't bake anything extra into the snapshot.
    'v8_use_external_startup_data%': 0,

    # Some STL containers (e.g. std::vector) do not preserve ABI compatibility
    # between debug and non-debug mode.
    'disable_glibcxx_debug': 1,

    # Don't use ICU data file (icudtl.dat) from V8, we use our own.
    'icu_use_data_file_flag%': 0,

    'conditions': [
      ['GENERATOR=="ninja"', {
        'OBJ_DIR': '<(PRODUCT_DIR)/obj',
        'V8_BASE': '<(PRODUCT_DIR)/obj/deps/v8/src/libv8_base.a',
       }, {
         'OBJ_DIR%': '<(PRODUCT_DIR)/obj.target',
         'V8_BASE%': '<(PRODUCT_DIR)/obj.target/deps/v8/src/libv8_base.a',
      }],
      ['OS == "win"', {
        'os_posix': 0,
        'v8_postmortem_support%': 'false',
        'OBJ_DIR': '<(PRODUCT_DIR)/obj',
        'V8_BASE': '<(PRODUCT_DIR)/lib/v8_libbase.lib',
      }, {
        'os_posix': 1,
        'v8_postmortem_support%': 'true',
      }],
      ['OS== "mac"', {
        'OBJ_DIR%': '<(PRODUCT_DIR)/obj.target',
        'V8_BASE': '<(PRODUCT_DIR)/libv8_base.a',
      }],
      ['openssl_fips != ""', {
        'OPENSSL_PRODUCT': '<(STATIC_LIB_PREFIX)crypto<(STATIC_LIB_SUFFIX)',
      }, {
        'OPENSSL_PRODUCT': '<(STATIC_LIB_PREFIX)openssl<(STATIC_LIB_SUFFIX)',
      }],
      ['OS=="mac"', {
        'clang%': 1,
      }, {
        'clang%': 0,
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
            'AdditionalOptions': [
              '/bigobj', # prevent error C1128 in VS2015
              '/MP', # compile across multiple CPUs
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
          }]
        ],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'Optimization': 3, # /Ox, full optimization
            'FavorSizeOrSpeed': 1, # /Ot, favor speed over size
            'InlineFunctionExpansion': 2, # /Ob2, inline anything eligible
            'WholeProgramOptimization': 'true', # /GL, whole program optimization, needed for LTCG
            'OmitFramePointers': 'true',
            'EnableFunctionLevelLinking': 'true',
            'EnableIntrinsicFunctions': 'true',
            'RuntimeTypeInfo': 'false',
            'AdditionalOptions': [
              '/MP', # compile across multiple CPUs
            ],
          },
          'VCLibrarianTool': {
            'AdditionalOptions': [
              '/LTCG', # link time code generation
            ],
          },
          'VCLinkerTool': {
            'OptimizeReferences': 2, # /OPT:REF
            'EnableCOMDATFolding': 2, # /OPT:ICF
            'LinkIncremental': 1, # disable incremental linking
            'AdditionalOptions': [
              '/LTCG:INCREMENTAL', # incremental link-time code generation
            ],
          },
        },
      }
    },
    # Forcibly disable -Werror.  We support a wide range of compilers, it's
    # simply not feasible to squelch all warnings, never mind that the
    # libraries in deps/ are not under our control.
    'cflags!': ['-Werror'],
    'msvs_settings': {
      'VCCLCompilerTool': {
        'StringPooling': 'true', # pool string literals
        'DebugInformationFormat': 3, # Generate a PDB
        'WarningLevel': 3,
        'BufferSecurityCheck': 'true',
        'ExceptionHandling': 0, # /EHsc
        'SuppressStartupBanner': 'true',
        # Disable "warning C4267: conversion from 'size_t' to 'int',
        # possible loss of data".  Many originate from our dependencies
        # and their sheer number drowns out other, more legitimate warnings.
        'DisableSpecificWarnings': ['4267'],
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
    'msvs_disabled_warnings': [4351, 4355, 4800],
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
        'cflags': [ '-pthread', ],
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
          [ 'OS=="aix"', {
            'conditions': [
              [ 'target_arch=="ppc"', {
                'ldflags': [ '-Wl,-bmaxdata:0x60000000/dsa' ],
              }],
              [ 'target_arch=="ppc64"', {
                'cflags': [ '-maix64' ],
                'ldflags': [ '-maix64' ],
              }],
            ],
            'ldflags': [ '-Wl,-bbigtoc' ],
            'ldflags!': [ '-rdynamic' ],
          }],
          [ 'node_shared=="true"', {
            'cflags': [ '-fPIC' ],
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
          'GCC_THREADSAFE_STATICS': 'NO',           # -fno-threadsafe-statics
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
        'conditions': [
          ['"0" < llvm_version < "4.0"', {
            # Use this flag because on FreeBSD std::pairs copy constructor is non-trivial.
            # Doesn't apply to llvm 4.0 (FreeBSD 11.1) or later.
            # Refs: https://lists.freebsd.org/pipermail/freebsd-toolchain/2016-March/002094.html
            # Refs: https://svnweb.freebsd.org/ports/head/www/node/Makefile?revision=444555&view=markup
            'cflags': [ '-D_LIBCPP_TRIVIAL_PAIR_COPY_CTOR=1' ],
          }],
        ],
        'ldflags': [
          '-Wl,--export-dynamic',
        ],
      }]
    ],
  }
}
