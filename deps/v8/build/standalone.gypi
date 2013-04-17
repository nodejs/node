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
  'variables': {
    'component%': 'static_library',
    'visibility%': 'hidden',
    'v8_enable_backtrace%': 0,
    'msvs_multi_core_compile%': '1',
    'mac_deployment_target%': '10.5',
    'variables': {
      'variables': {
        'variables': {
          'conditions': [
            ['OS=="linux" or OS=="freebsd" or OS=="openbsd" or \
               OS=="netbsd" or OS=="mac"', {
              # This handles the Unix platforms we generally deal with.
              # Anything else gets passed through, which probably won't work
              # very well; such hosts should pass an explicit target_arch
              # to gyp.
              'host_arch%':
                '<!(uname -m | sed -e "s/i.86/ia32/;\
                  s/x86_64/x64/;s/amd64/x64/;s/arm.*/arm/;s/mips.*/mipsel/")',
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
    'conditions': [
      ['(v8_target_arch=="arm" and host_arch!="arm") or \
        (v8_target_arch=="mipsel" and host_arch!="mipsel") or \
        (v8_target_arch=="x64" and host_arch!="x64") or \
        (OS=="android")', {
        'want_separate_host_toolset': 1,
      }, {
        'want_separate_host_toolset': 0,
      }],
    ],
    # Default ARM variable settings.
    'armv7%': 'default',
    'arm_neon%': 0,
    'arm_fpu%': 'vfpv3',
    'arm_float_abi%': 'default',
    'arm_thumb': 'default',
  },
  'target_defaults': {
    'default_configuration': 'Debug',
    'configurations': {
      'Debug': {
        'cflags': [ '-g', '-O0' ],
      },
      'Release': {
        # Xcode insists on this empty entry.
      },
    },
  },
  'conditions': [
    ['OS=="linux" or OS=="freebsd" or OS=="openbsd" or OS=="solaris" \
       or OS=="netbsd"', {
      'target_defaults': {
        'cflags': [ '-Wall', '<(werror)', '-W', '-Wno-unused-parameter',
                    '-Wnon-virtual-dtor', '-pthread', '-fno-rtti',
                    '-fno-exceptions', '-pedantic' ],
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
          'GCC_TREAT_WARNINGS_AS_ERRORS': 'YES',    # -Werror
          'GCC_VERSION': 'com.apple.compilers.llvmgcc42',
          'GCC_WARN_ABOUT_MISSING_NEWLINE': 'YES',  # -Wnewline-eof
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
            '-Wnon-virtual-dtor',
          ],
        },
        'target_conditions': [
          ['_type!="static_library"', {
            'xcode_settings': {'OTHER_LDFLAGS': ['-Wl,-search_paths_first']},
          }],
        ],  # target_conditions
      },  # target_defaults
    }],  # OS=="mac"
  ],
}
