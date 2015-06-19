# Copyright 2012 the V8 project authors. All rights reserved.
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#     * Neither the name of Google Inc. nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Definitions to be used when building stand-alone V8 binaries.

{
  # We need to include toolchain.gypi here for third-party sources that don't
  # directly include it themselves.
  'includes': ['toolchain.gypi'],
  'variables': {
    'component%': 'static_library',
    'clang_dir%': 'third_party/llvm-build/Release+Asserts',
    'clang_xcode%': 0,
    # Track where uninitialized memory originates from. From fastest to
    # slowest: 0 - no tracking, 1 - track only the initial allocation site, 2
    # - track the chain of stores leading from allocation site to use site.
    'msan_track_origins%': 1,
    'visibility%': 'hidden',
    'v8_enable_backtrace%': 0,
    'v8_enable_i18n_support%': 1,
    'v8_deprecation_warnings': 1,
    'msvs_multi_core_compile%': '1',
    'mac_deployment_target%': '10.5',
    'release_extra_cflags%': '',
    'variables': {
      'variables': {
        'variables': {
          'conditions': [
            ['OS=="linux" or OS=="freebsd" or OS=="openbsd" or \
               OS=="netbsd" or OS=="mac" or OS=="qnx" or OS=="aix"', {
              # This handles the Unix platforms we generally deal with.
              # Anything else gets passed through, which probably won't work
              # very well; such hosts should pass an explicit target_arch
              # to gyp.
              'host_arch%': '<!pymod_do_main(detect_v8_host_arch)',
            }, {
              # OS!="linux" and OS!="freebsd" and OS!="openbsd" and
              # OS!="netbsd" and OS!="mac" and OS!="aix"
              'host_arch%': 'ia32',
            }],
          ],
        },
        'host_arch%': '<(host_arch)',
        'target_arch%': '<(host_arch)',
      },
      'host_arch%': '<(host_arch)',
      'target_arch%': '<(target_arch)',
      'v8_target_arch%': '<(target_arch)',
      'asan%': 0,
      'lsan%': 0,
      'msan%': 0,
      'tsan%': 0,

      # goma settings.
      # 1 to use goma.
      # If no gomadir is set, it uses the default gomadir.
      'use_goma%': 0,
      'gomadir%': '',
      'conditions': [
        # Set default gomadir.
        ['OS=="win"', {
          'gomadir': 'c:\\goma\\goma-win',
        }, {
          'gomadir': '<!(/bin/echo -n ${HOME}/goma)',
        }],
      ],
    },
    'host_arch%': '<(host_arch)',
    'target_arch%': '<(target_arch)',
    'v8_target_arch%': '<(v8_target_arch)',
    'werror%': '-Werror',
    'use_goma%': '<(use_goma)',
    'gomadir%': '<(gomadir)',
    'asan%': '<(asan)',
    'lsan%': '<(lsan)',
    'msan%': '<(msan)',
    'tsan%': '<(tsan)',

    # .gyp files or targets should set v8_code to 1 if they build V8 specific
    # code, as opposed to external code.  This variable is used to control such
    # things as the set of warnings to enable, and whether warnings are treated
    # as errors.
    'v8_code%': 0,

    # Speeds up Debug builds:
    # 0 - Compiler optimizations off (debuggable) (default). This may
    #     be 5x slower than Release (or worse).
    # 1 - Turn on optimizations and disable slow DCHECKs, but leave
    #     V8_ENABLE_CHECKS and most other assertions enabled.  This may cause
    #     some v8 tests to fail in the Debug configuration.  This roughly
    #     matches the performance of a Release build and can be used by
    #     embedders that need to build their own code as debug but don't want
    #     or need a debug version of V8. This should produce near-release
    #     speeds.
    'v8_optimized_debug%': 0,

    # Use external files for startup data blobs:
    # the JS builtins sources and the start snapshot.
    # Embedders that don't use standalone.gypi will need to add
    # their own default value.
    'v8_use_external_startup_data%': 0,

    # Relative path to icu.gyp from this file.
    'icu_gyp_path': '../third_party/icu/icu.gyp',

    'conditions': [
      ['(v8_target_arch=="arm" and host_arch!="arm") or \
        (v8_target_arch=="arm64" and host_arch!="arm64") or \
        (v8_target_arch=="mipsel" and host_arch!="mipsel") or \
        (v8_target_arch=="mips64el" and host_arch!="mips64el") or \
        (v8_target_arch=="x64" and host_arch!="x64") or \
        (OS=="android" or OS=="qnx")', {
        'want_separate_host_toolset': 1,
      }, {
        'want_separate_host_toolset': 0,
      }],
      ['OS == "win"', {
        'os_posix%': 0,
      }, {
        'os_posix%': 1,
      }],
      ['OS=="win" and use_goma==1', {
        # goma doesn't support pch yet.
        'chromium_win_pch': 0,
        # goma doesn't support PDB yet, so win_z7=1 or fastbuild=1.
        'conditions': [
          ['win_z7==0 and fastbuild==0', {
            'fastbuild': 1,
          }],
        ],
      }],
      ['(v8_target_arch=="ia32" or v8_target_arch=="x64" or v8_target_arch=="x87") and \
        (OS=="linux" or OS=="mac")', {
        'v8_enable_gdbjit%': 1,
      }, {
        'v8_enable_gdbjit%': 0,
      }],
      ['(OS=="linux" or OS=="mac") and (target_arch=="ia32" or target_arch=="x64") and \
        (v8_target_arch!="x87")', {
        'clang%': 1,
      }, {
        'clang%': 0,
      }],
      ['host_arch!="ppc" and host_arch!="ppc64" and host_arch!="ppc64le"', {
        'host_clang%': '1',
      }, {
        'host_clang%': '0',
      }],
      ['asan==1 or lsan==1 or msan==1 or tsan==1', {
        'clang%': 1,
        'use_allocator%': 'none',
      }],
    ],
    # Default ARM variable settings.
    'arm_version%': 'default',
    'arm_fpu%': 'vfpv3',
    'arm_float_abi%': 'default',
    'arm_thumb': 'default',

    # Default MIPS variable settings.
    'mips_arch_variant%': 'r2',
    # Possible values fp32, fp64, fpxx.
    # fp32 - 32 32-bit FPU registers are available, doubles are placed in
    #        register pairs.
    # fp64 - 32 64-bit FPU registers are available.
    # fpxx - compatibility mode, it chooses fp32 or fp64 depending on runtime
    #        detection
    'mips_fpu_mode%': 'fp32',
  },
  'target_defaults': {
    'variables': {
      'v8_code%': '<(v8_code)',
    },
    'default_configuration': 'Debug',
    'configurations': {
      'DebugBaseCommon': {
        'conditions': [
          ['OS=="aix"', {
            'cflags': [ '-g', '-Og', '-gxcoff' ],
          }, {
            'cflags': [ '-g', '-O0' ],
          }],
        ],
      },
      'Optdebug': {
        'inherit_from': [ 'DebugBaseCommon', 'DebugBase1' ],
      },
      'Debug': {
        # Xcode insists on this empty entry.
      },
      'Release': {
        'cflags+': ['<@(release_extra_cflags)'],
      },
    },
    'conditions':[
      ['(clang==1 or host_clang==1) and OS!="win"', {
        # This is here so that all files get recompiled after a clang roll and
        # when turning clang on or off.
        # (defines are passed via the command line, and build systems rebuild
        # things when their commandline changes). Nothing should ever read this
        # define.
        'defines': ['CR_CLANG_REVISION=<!(<(DEPTH)/tools/clang/scripts/update.sh --print-revision)'],
        'cflags+': [
          '-Wno-format-pedantic',
        ],
      }],
    ],
    'target_conditions': [
      ['v8_code == 0', {
        'defines!': [
          'DEBUG',
        ],
        'conditions': [
          ['os_posix == 1 and OS != "mac"', {
            # We don't want to get warnings from third-party code,
            # so remove any existing warning-enabling flags like -Wall.
            'cflags!': [
              '-pedantic',
              '-Wall',
              '-Werror',
              '-Wextra',
              '-Wshorten-64-to-32',
            ],
            'cflags+': [
              # Clang considers the `register` keyword as deprecated, but
              # ICU uses it all over the place.
              '-Wno-deprecated-register',
              # ICU uses its own deprecated functions.
              '-Wno-deprecated-declarations',
              # ICU prefers `a && b || c` over `(a && b) || c`.
              '-Wno-logical-op-parentheses',
              # ICU has some `unsigned < 0` checks.
              '-Wno-tautological-compare',
              # uresdata.c has switch(RES_GET_TYPE(x)) code. The
              # RES_GET_TYPE macro returns an UResType enum, but some switch
              # statement contains case values that aren't part of that
              # enum (e.g. URES_TABLE32 which is in UResInternalType). This
              # is on purpose.
              '-Wno-switch',
            ],
            'cflags_cc!': [
              '-Wnon-virtual-dtor',
            ],
          }],
          ['OS == "mac"', {
            'xcode_settings': {
              'GCC_TREAT_WARNINGS_AS_ERRORS': 'NO',    # -Werror
            },
          }],
          ['OS == "win"', {
            'msvs_settings': {
              'VCCLCompilerTool': {
                'WarnAsError': 'false',
              },
            },
          }],
        ],
      }],
    ],
  },
  'conditions': [
    ['asan==1 and OS!="mac"', {
      'target_defaults': {
        'cflags_cc+': [
          '-fno-omit-frame-pointer',
          '-gline-tables-only',
          '-fsanitize=address',
          '-w',  # http://crbug.com/162783
        ],
        'cflags!': [
          '-fomit-frame-pointer',
        ],
        'ldflags': [
          '-fsanitize=address',
        ],
      },
    }],
    ['tsan==1 and OS!="mac"', {
      'target_defaults': {
        'cflags+': [
          '-fno-omit-frame-pointer',
          '-gline-tables-only',
          '-fsanitize=thread',
          '-fPIC',
          '-Wno-c++11-extensions',
        ],
        'cflags!': [
          '-fomit-frame-pointer',
        ],
        'ldflags': [
          '-fsanitize=thread',
          '-pie',
        ],
        'defines': [
          'THREAD_SANITIZER',
        ],
      },
    }],
    ['msan==1 and OS!="mac"', {
      'target_defaults': {
        'cflags_cc+': [
          '-fno-omit-frame-pointer',
          '-gline-tables-only',
          '-fsanitize=memory',
          '-fsanitize-memory-track-origins=<(msan_track_origins)',
          '-fPIC',
        ],
        'cflags+': [
          '-fPIC',
        ],
        'cflags!': [
          '-fno-exceptions',
          '-fomit-frame-pointer',
        ],
        'ldflags': [
          '-fsanitize=memory',
        ],
        'defines': [
          'MEMORY_SANITIZER',
        ],
        'dependencies': [
          # Use libc++ (third_party/libc++ and third_party/libc++abi) instead of
          # stdlibc++ as standard library. This is intended to use for instrumented
          # builds.
          '<(DEPTH)/buildtools/third_party/libc++/libc++.gyp:libcxx_proxy',
        ],
      },
    }],
    ['asan==1 and OS=="mac"', {
      'target_defaults': {
        'xcode_settings': {
          'OTHER_CFLAGS+': [
            '-fno-omit-frame-pointer',
            '-gline-tables-only',
            '-fsanitize=address',
            '-w',  # http://crbug.com/162783
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
        'dependencies': [
          '<(DEPTH)/build/mac/asan.gyp:asan_dynamic_runtime',
        ],
      },
    }],
    ['OS=="linux" or OS=="freebsd" or OS=="openbsd" or OS=="solaris" \
       or OS=="netbsd" or OS=="aix"', {
      'target_defaults': {
        'cflags': [
          '-Wall',
          '<(werror)',
          '-Wno-unused-parameter',
          '-Wno-long-long',
          '-pthread',
          '-fno-exceptions',
          '-pedantic',
          # Don't warn about the "struct foo f = {0};" initialization pattern.
          '-Wno-missing-field-initializers',
        ],
        'cflags_cc': [ '-Wnon-virtual-dtor', '-fno-rtti', '-std=gnu++0x' ],
        'ldflags': [ '-pthread', ],
        'conditions': [
          # TODO(arm64): It'd be nice to enable this for arm64 as well,
          # but the Assembler requires some serious fixing first.
          [ 'clang==1 and v8_target_arch=="x64"', {
            'cflags': [ '-Wshorten-64-to-32' ],
          }],
          [ 'host_arch=="ppc64" and OS!="aix"', {
            'cflags': [ '-mminimal-toc' ],
          }],
          [ 'visibility=="hidden" and v8_enable_backtrace==0', {
            'cflags': [ '-fvisibility=hidden' ],
          }],
          [ 'component=="shared_library"', {
            'cflags': [ '-fPIC', ],
          }],
        ],
      },
    }],
    # 'OS=="linux" or OS=="freebsd" or OS=="openbsd" or OS=="solaris"
    #  or OS=="netbsd"'
    ['OS=="qnx"', {
      'target_defaults': {
        'cflags': [
          '-Wall',
          '<(werror)',
          '-Wno-unused-parameter',
          '-fno-exceptions',
          # Don't warn about the "struct foo f = {0};" initialization pattern.
          '-Wno-missing-field-initializers',
        ],
        'cflags_cc': [ '-Wnon-virtual-dtor', '-fno-rtti', '-std=gnu++0x' ],
        'conditions': [
          [ 'visibility=="hidden"', {
            'cflags': [ '-fvisibility=hidden' ],
          }],
          [ 'component=="shared_library"', {
            'cflags': [ '-fPIC' ],
          }],
        ],
        'target_conditions': [
          [ '_toolset=="host" and host_os=="linux"', {
            'cflags': [ '-pthread' ],
            'ldflags': [ '-pthread' ],
            'libraries': [ '-lrt' ],
          }],
          [ '_toolset=="target"', {
            'cflags': [ '-Wno-psabi' ],
            'libraries': [ '-lbacktrace', '-lsocket', '-lm' ],
          }],
        ],
      },
    }],  # OS=="qnx"
    ['OS=="win"', {
      'target_defaults': {
        'defines': [
          '_CRT_SECURE_NO_DEPRECATE',
          '_CRT_NONSTDC_NO_DEPRECATE',
          '_USING_V110_SDK71_',
        ],
        'conditions': [
          ['component=="static_library"', {
            'defines': [
              '_HAS_EXCEPTIONS=0',
            ],
          }],
        ],
        'msvs_cygwin_dirs': ['<(DEPTH)/third_party/cygwin'],
        'msvs_disabled_warnings': [4355, 4800],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'MinimalRebuild': 'false',
            'BufferSecurityCheck': 'true',
            'EnableFunctionLevelLinking': 'true',
            'RuntimeTypeInfo': 'false',
            'WarningLevel': '3',
            'WarnAsError': 'true',
            'DebugInformationFormat': '3',
            'Detect64BitPortabilityProblems': 'false',
            'conditions': [
              [ 'msvs_multi_core_compile', {
                'AdditionalOptions': ['/MP'],
              }],
              ['component=="shared_library"', {
                'ExceptionHandling': '1',  # /EHsc
              }, {
                'ExceptionHandling': '0',
              }],
            ],
          },
          'VCLibrarianTool': {
            'AdditionalOptions': ['/ignore:4221'],
            'conditions': [
              ['v8_target_arch=="x64"', {
                'TargetMachine': '17',  # x64
              }, {
                'TargetMachine': '1',  # ia32
              }],
            ],
          },
          'VCLinkerTool': {
            'AdditionalDependencies': [
              'ws2_32.lib',
            ],
            'GenerateDebugInformation': 'true',
            'MapFileName': '$(OutDir)\\$(TargetName).map',
            'ImportLibrary': '$(OutDir)\\lib\\$(TargetName).lib',
            'FixedBaseAddress': '1',
            # LinkIncremental values:
            #   0 == default
            #   1 == /INCREMENTAL:NO
            #   2 == /INCREMENTAL
            'LinkIncremental': '1',
            # SubSystem values:
            #   0 == not set
            #   1 == /SUBSYSTEM:CONSOLE
            #   2 == /SUBSYSTEM:WINDOWS
            'SubSystem': '1',

            'conditions': [
              ['v8_enable_i18n_support==1', {
                'AdditionalDependencies': [
                  'advapi32.lib',
                ],
              }],
              ['v8_target_arch=="x64"', {
                'MinimumRequiredVersion': '5.02',  # Server 2003.
                'TargetMachine': '17',  # x64
              }, {
                'MinimumRequiredVersion': '5.01',  # XP.
                'TargetMachine': '1',  # ia32
              }],
            ],
          },
        },
      },
    }],  # OS=="win"
    ['OS=="mac"', {
      'xcode_settings': {
        'SDKROOT': 'macosx',
        'SYMROOT': '<(DEPTH)/xcodebuild',
      },
      'target_defaults': {
        'xcode_settings': {
          'ALWAYS_SEARCH_USER_PATHS': 'NO',
          'GCC_C_LANGUAGE_STANDARD': 'c99',         # -std=c99
          'GCC_CW_ASM_SYNTAX': 'NO',                # No -fasm-blocks
          'GCC_DYNAMIC_NO_PIC': 'NO',               # No -mdynamic-no-pic
                                                    # (Equivalent to -fPIC)
          'GCC_ENABLE_CPP_EXCEPTIONS': 'NO',        # -fno-exceptions
          'GCC_ENABLE_CPP_RTTI': 'NO',              # -fno-rtti
          'GCC_ENABLE_PASCAL_STRINGS': 'NO',        # No -mpascal-strings
          # GCC_INLINES_ARE_PRIVATE_EXTERN maps to -fvisibility-inlines-hidden
          'GCC_INLINES_ARE_PRIVATE_EXTERN': 'YES',
          'GCC_SYMBOLS_PRIVATE_EXTERN': 'YES',      # -fvisibility=hidden
          'GCC_THREADSAFE_STATICS': 'NO',           # -fno-threadsafe-statics
          'GCC_WARN_NON_VIRTUAL_DESTRUCTOR': 'YES', # -Wnon-virtual-dtor
          # MACOSX_DEPLOYMENT_TARGET maps to -mmacosx-version-min
          'MACOSX_DEPLOYMENT_TARGET': '<(mac_deployment_target)',
          'PREBINDING': 'NO',                       # No -Wl,-prebind
          'SYMROOT': '<(DEPTH)/xcodebuild',
          'USE_HEADERMAP': 'NO',
          'OTHER_CFLAGS': [
            '-fno-strict-aliasing',
          ],
          'WARNING_CFLAGS': [
            '-Wall',
            '-Wendif-labels',
            '-Wno-unused-parameter',
            # Don't warn about the "struct foo f = {0};" initialization pattern.
            '-Wno-missing-field-initializers',
          ],
        },
        'conditions': [
          ['werror==""', {
            'xcode_settings': {'GCC_TREAT_WARNINGS_AS_ERRORS': 'NO'},
          }, {
            'xcode_settings': {'GCC_TREAT_WARNINGS_AS_ERRORS': 'YES'},
          }],
          ['clang==1', {
            'xcode_settings': {
              'GCC_VERSION': 'com.apple.compilers.llvm.clang.1_0',
              'CLANG_CXX_LANGUAGE_STANDARD': 'gnu++0x',  # -std=gnu++0x
            },
          }],
        ],
        'target_conditions': [
          ['_type!="static_library"', {
            'xcode_settings': {'OTHER_LDFLAGS': ['-Wl,-search_paths_first']},
          }],
        ],  # target_conditions
      },  # target_defaults
    }],  # OS=="mac"
    ['clang!=1 and host_clang==1 and target_arch!="ia32" and target_arch!="x64"', {
      'make_global_settings': [
        ['CC.host', '../<(clang_dir)/bin/clang'],
        ['CXX.host', '../<(clang_dir)/bin/clang++'],
      ],
    }],
    ['clang==0 and host_clang==1 and target_arch!="ia32" and target_arch!="x64"', {
      'target_conditions': [
        ['_toolset=="host"', {
          'cflags_cc': [ '-std=gnu++11', ],
        }],
      ],
      'target_defaults': {
        'target_conditions': [
          ['_toolset=="host"', { 'cflags!': [ '-Wno-unused-local-typedefs' ]}],
        ],
      },
    }],
    ['clang==1 and "<(GENERATOR)"=="ninja"', {
      # See http://crbug.com/110262
      'target_defaults': {
        'cflags': [ '-fcolor-diagnostics' ],
        'xcode_settings': { 'OTHER_CFLAGS': [ '-fcolor-diagnostics' ] },
      },
    }],
    ['clang==1 and ((OS!="mac" and OS!="ios") or clang_xcode==0) '
        'and OS!="win" and "<(GENERATOR)"=="make"', {
      'make_global_settings': [
        ['CC', '../<(clang_dir)/bin/clang'],
        ['CXX', '../<(clang_dir)/bin/clang++'],
        ['CC.host', '$(CC)'],
        ['CXX.host', '$(CXX)'],
      ],
    }],
    ['clang==1 and ((OS!="mac" and OS!="ios") or clang_xcode==0) '
        'and OS!="win" and "<(GENERATOR)"=="ninja"', {
      'make_global_settings': [
        ['CC', '<(clang_dir)/bin/clang'],
        ['CXX', '<(clang_dir)/bin/clang++'],
        ['CC.host', '$(CC)'],
        ['CXX.host', '$(CXX)'],
      ],
    }],
    ['clang==1 and OS=="win"', {
      'make_global_settings': [
        # On Windows, gyp's ninja generator only looks at CC.
        ['CC', '../<(clang_dir)/bin/clang-cl'],
      ],
    }],
    # TODO(yyanagisawa): supports GENERATOR==make
    #  make generator doesn't support CC_wrapper without CC
    #  in make_global_settings yet.
    ['use_goma==1 and ("<(GENERATOR)"=="ninja" or clang==1)', {
      'make_global_settings': [
       ['CC_wrapper', '<(gomadir)/gomacc'],
       ['CXX_wrapper', '<(gomadir)/gomacc'],
       ['CC.host_wrapper', '<(gomadir)/gomacc'],
       ['CXX.host_wrapper', '<(gomadir)/gomacc'],
      ],
    }],
  ],
}
