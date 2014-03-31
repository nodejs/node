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
    'clang%': 0,
    'asan%': 0,
    'visibility%': 'hidden',
    'v8_enable_backtrace%': 0,
    'v8_enable_i18n_support%': 1,
    'v8_deprecation_warnings': 1,
    'msvs_multi_core_compile%': '1',
    'mac_deployment_target%': '10.5',
    'variables': {
      'variables': {
        'variables': {
          'conditions': [
            ['OS=="linux" or OS=="freebsd" or OS=="openbsd" or \
               OS=="netbsd" or OS=="mac" or OS=="qnx"', {
              # This handles the Unix platforms we generally deal with.
              # Anything else gets passed through, which probably won't work
              # very well; such hosts should pass an explicit target_arch
              # to gyp.
              'host_arch%':
                '<!(uname -m | sed -e "s/i.86/ia32/;\
                                       s/x86_64/x64/;\
                                       s/amd64/x64/;\
                                       s/aarch64/arm64/;\
                                       s/arm.*/arm/;\
                                       s/mips.*/mipsel/")',
            }, {
              # OS!="linux" and OS!="freebsd" and OS!="openbsd" and
              # OS!="netbsd" and OS!="mac"
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
    },
    'host_arch%': '<(host_arch)',
    'target_arch%': '<(target_arch)',
    'v8_target_arch%': '<(v8_target_arch)',
    'werror%': '-Werror',

    # .gyp files or targets should set v8_code to 1 if they build V8 specific
    # code, as opposed to external code.  This variable is used to control such
    # things as the set of warnings to enable, and whether warnings are treated
    # as errors.
    'v8_code%': 0,

    # Speeds up Debug builds:
    # 0 - Compiler optimizations off (debuggable) (default). This may
    #     be 5x slower than Release (or worse).
    # 1 - Turn on compiler optimizations. This may be hard or impossible to
    #     debug. This may still be 2x slower than Release (or worse).
    # 2 - Turn on optimizations, and also #undef DEBUG / #define NDEBUG
    #     (but leave V8_ENABLE_CHECKS and most other assertions enabled.
    #     This may cause some v8 tests to fail in the Debug configuration.
    #     This roughly matches the performance of a Release build and can
    #     be used by embedders that need to build their own code as debug
    #     but don't want or need a debug version of V8. This should produce
    #     near-release speeds.
    'v8_optimized_debug%': 0,

    # Relative path to icu.gyp from this file.
    'icu_gyp_path': '../third_party/icu/icu.gyp',

    'conditions': [
      ['(v8_target_arch=="arm" and host_arch!="arm") or \
        (v8_target_arch=="arm64" and host_arch!="arm64") or \
        (v8_target_arch=="mipsel" and host_arch!="mipsel") or \
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
      ['(v8_target_arch=="ia32" or v8_target_arch=="x64") and \
        (OS=="linux" or OS=="mac")', {
        'v8_enable_gdbjit%': 1,
      }, {
        'v8_enable_gdbjit%': 0,
      }],
    ],
    # Default ARM variable settings.
    'arm_version%': 'default',
    'arm_neon%': 0,
    'arm_fpu%': 'vfpv3',
    'arm_float_abi%': 'default',
    'arm_thumb': 'default',
  },
  'target_defaults': {
    'variables': {
      'v8_code%': '<(v8_code)',
    },
    'default_configuration': 'Debug',
    'configurations': {
      'Debug': {
        'cflags': [ '-g', '-O0' ],
      },
      'Release': {
        # Xcode insists on this empty entry.
      },
    },
    'target_conditions': [
      ['v8_code == 0', {
        'defines!': [
          'DEBUG',
        ],
        'conditions': [
          ['os_posix == 1 and OS != "mac"', {
            'cflags!': [
              '-Werror',
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
    ['asan==1', {
      'target_defaults': {
        'cflags_cc+': [
          '-fno-omit-frame-pointer',
          '-gline-tables-only',
          '-fsanitize=address',
          '-w',  # http://crbug.com/162783
        ],
        'cflags_cc!': [
          '-fomit-frame-pointer',
        ],
        'ldflags': [
          '-fsanitize=address',
        ],
      },
    }],
    ['OS=="linux" or OS=="freebsd" or OS=="openbsd" or OS=="solaris" \
       or OS=="netbsd"', {
      'target_defaults': {
        'cflags': [ '-Wall', '<(werror)', '-W', '-Wno-unused-parameter',
                    '-pthread', '-fno-exceptions', '-pedantic' ],
        'cflags_cc': [ '-Wnon-virtual-dtor', '-fno-rtti' ],
        'ldflags': [ '-pthread', ],
        'conditions': [
          [ 'OS=="linux"', {
            'cflags': [ '-ansi' ],
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
        'cflags': [ '-Wall', '<(werror)', '-W', '-Wno-unused-parameter',
                    '-fno-exceptions' ],
        'cflags_cc': [ '-Wnon-virtual-dtor', '-fno-rtti' ],
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
            ],
          },
        },
      },
    }],  # OS=="win"
    ['OS=="mac"', {
      'xcode_settings': {
        'SYMROOT': '<(DEPTH)/xcodebuild',
      },
      'target_defaults': {
        'xcode_settings': {
          'ALWAYS_SEARCH_USER_PATHS': 'NO',
          'GCC_C_LANGUAGE_STANDARD': 'ansi',        # -ansi
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
          'GCC_WARN_ABOUT_MISSING_NEWLINE': 'YES',  # -Wnewline-eof
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
            '-W',
            '-Wno-unused-parameter',
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
              'CLANG_CXX_LANGUAGE_STANDARD': 'gnu++11',  # -std=gnu++11
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
  ],
}
