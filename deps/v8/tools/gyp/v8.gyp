# Copyright 2009 the V8 project authors. All rights reserved.
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

{
  'variables': {
    'chromium_code': 1,
    'msvs_use_common_release': 0,
    'base_source_files': [
      '../../src/arm/assembler-arm-inl.h',
      '../../src/arm/assembler-arm.cc',
      '../../src/arm/assembler-arm.h',
      '../../src/arm/builtins-arm.cc',
      '../../src/arm/codegen-arm.cc',
      '../../src/arm/codegen-arm.h',
      '../../src/arm/constants-arm.h',
      '../../src/arm/cpu-arm.cc',
      '../../src/arm/debug-arm.cc',
      '../../src/arm/disasm-arm.cc',
      '../../src/arm/frames-arm.cc',
      '../../src/arm/frames-arm.h',
      '../../src/arm/ic-arm.cc',
      '../../src/arm/jump-target-arm.cc',
      '../../src/arm/macro-assembler-arm.cc',
      '../../src/arm/macro-assembler-arm.h',
      '../../src/arm/regexp-macro-assembler-arm.cc',
      '../../src/arm/regexp-macro-assembler-arm.h',
      '../../src/arm/register-allocator-arm.cc',
      '../../src/arm/simulator-arm.cc',
      '../../src/arm/stub-cache-arm.cc',
      '../../src/arm/virtual-frame-arm.cc',
      '../../src/arm/virtual-frame-arm.h',
      '../../src/ia32/assembler-ia32-inl.h',
      '../../src/ia32/assembler-ia32.cc',
      '../../src/ia32/assembler-ia32.h',
      '../../src/ia32/builtins-ia32.cc',
      '../../src/ia32/codegen-ia32.cc',
      '../../src/ia32/codegen-ia32.h',
      '../../src/ia32/cpu-ia32.cc',
      '../../src/ia32/debug-ia32.cc',
      '../../src/ia32/disasm-ia32.cc',
      '../../src/ia32/frames-ia32.cc',
      '../../src/ia32/frames-ia32.h',
      '../../src/ia32/ic-ia32.cc',
      '../../src/ia32/jump-target-ia32.cc',
      '../../src/ia32/macro-assembler-ia32.cc',
      '../../src/ia32/macro-assembler-ia32.h',
      '../../src/ia32/regexp-macro-assembler-ia32.cc',
      '../../src/ia32/regexp-macro-assembler-ia32.h',
      '../../src/ia32/register-allocator-ia32.cc',
      '../../src/ia32/stub-cache-ia32.cc',
      '../../src/ia32/virtual-frame-ia32.cc',
      '../../src/ia32/virtual-frame-ia32.h',
      '../../src/third_party/dtoa/dtoa.c',
      '../../src/accessors.cc',
      '../../src/accessors.h',
      '../../src/allocation.cc',
      '../../src/allocation.h',
      '../../src/api.cc',
      '../../src/api.h',
      '../../src/apiutils.h',
      '../../src/arguments.h',
      '../../src/assembler.cc',
      '../../src/assembler.h',
      '../../src/ast.cc',
      '../../src/ast.h',
      '../../src/bootstrapper.cc',
      '../../src/bootstrapper.h',
      '../../src/builtins.cc',
      '../../src/builtins.h',
      '../../src/bytecodes-irregexp.h',
      '../../src/char-predicates-inl.h',
      '../../src/char-predicates.h',
      '../../src/checks.cc',
      '../../src/checks.h',
      '../../src/code-stubs.cc',
      '../../src/code-stubs.h',
      '../../src/code.h',
      '../../src/codegen-inl.h',
      '../../src/codegen.cc',
      '../../src/codegen.h',
      '../../src/compilation-cache.cc',
      '../../src/compilation-cache.h',
      '../../src/compiler.cc',
      '../../src/compiler.h',
      '../../src/contexts.cc',
      '../../src/contexts.h',
      '../../src/conversions-inl.h',
      '../../src/conversions.cc',
      '../../src/conversions.h',
      '../../src/counters.cc',
      '../../src/counters.h',
      '../../src/cpu.h',
      '../../src/dateparser.cc',
      '../../src/dateparser.h',
      '../../src/dateparser-inl.h',
      '../../src/debug.cc',
      '../../src/debug.h',
      '../../src/debug-agent.cc',
      '../../src/debug-agent.h',
      '../../src/disasm.h',
      '../../src/disassembler.cc',
      '../../src/disassembler.h',
      '../../src/dtoa-config.c',
      '../../src/execution.cc',
      '../../src/execution.h',
      '../../src/factory.cc',
      '../../src/factory.h',
      '../../src/flag-definitions.h',
      '../../src/flags.cc',
      '../../src/flags.h',
      '../../src/frames-inl.h',
      '../../src/frames.cc',
      '../../src/frames.h',
      '../../src/func-name-inferrer.cc',
      '../../src/func-name-inferrer.h',
      '../../src/global-handles.cc',
      '../../src/global-handles.h',
      '../../src/globals.h',
      '../../src/handles-inl.h',
      '../../src/handles.cc',
      '../../src/handles.h',
      '../../src/hashmap.cc',
      '../../src/hashmap.h',
      '../../src/heap-inl.h',
      '../../src/heap.cc',
      '../../src/heap.h',
      '../../src/ic-inl.h',
      '../../src/ic.cc',
      '../../src/ic.h',
      '../../src/interpreter-irregexp.cc',
      '../../src/interpreter-irregexp.h',
      '../../src/jump-target.cc',
      '../../src/jump-target.h',
      '../../src/jsregexp-inl.h',
      '../../src/jsregexp.cc',
      '../../src/jsregexp.h',
      '../../src/list-inl.h',
      '../../src/list.h',
      '../../src/log.cc',
      '../../src/log.h',
      '../../src/macro-assembler.h',
      '../../src/mark-compact.cc',
      '../../src/mark-compact.h',
      '../../src/memory.h',
      '../../src/messages.cc',
      '../../src/messages.h',
      '../../src/natives.h',
      '../../src/objects-debug.cc',
      '../../src/objects-inl.h',
      '../../src/objects.cc',
      '../../src/objects.h',
      '../../src/oprofile-agent.h',
      '../../src/oprofile-agent.cc',
      '../../src/parser.cc',
      '../../src/parser.h',
      '../../src/platform-freebsd.cc',
      '../../src/platform-linux.cc',
      '../../src/platform-macos.cc',
      '../../src/platform-nullos.cc',
      '../../src/platform-posix.cc',
      '../../src/platform-win32.cc',
      '../../src/platform.h',
      '../../src/prettyprinter.cc',
      '../../src/prettyprinter.h',
      '../../src/property.cc',
      '../../src/property.h',
      '../../src/regexp-macro-assembler-irregexp-inl.h',
      '../../src/regexp-macro-assembler-irregexp.cc',
      '../../src/regexp-macro-assembler-irregexp.h',
      '../../src/regexp-macro-assembler-tracer.cc',
      '../../src/regexp-macro-assembler-tracer.h',
      '../../src/regexp-macro-assembler.cc',
      '../../src/regexp-macro-assembler.h',
      '../../src/regexp-stack.cc',
      '../../src/regexp-stack.h',
      '../../src/register-allocator.h',
      '../../src/register-allocator-inl.h',
      '../../src/register-allocator.cc',
      '../../src/rewriter.cc',
      '../../src/rewriter.h',
      '../../src/runtime.cc',
      '../../src/runtime.h',
      '../../src/scanner.cc',
      '../../src/scanner.h',
      '../../src/scopeinfo.cc',
      '../../src/scopeinfo.h',
      '../../src/scopes.cc',
      '../../src/scopes.h',
      '../../src/serialize.cc',
      '../../src/serialize.h',
      '../../src/shell.h',
      '../../src/smart-pointer.h',
      '../../src/snapshot-common.cc',
      '../../src/snapshot.h',
      '../../src/spaces-inl.h',
      '../../src/spaces.cc',
      '../../src/spaces.h',
      '../../src/string-stream.cc',
      '../../src/string-stream.h',
      '../../src/stub-cache.cc',
      '../../src/stub-cache.h',
      '../../src/token.cc',
      '../../src/token.h',
      '../../src/top.cc',
      '../../src/top.h',
      '../../src/unicode-inl.h',
      '../../src/unicode.cc',
      '../../src/unicode.h',
      '../../src/usage-analyzer.cc',
      '../../src/usage-analyzer.h',
      '../../src/utils.cc',
      '../../src/utils.h',
      '../../src/v8-counters.cc',
      '../../src/v8-counters.h',
      '../../src/v8.cc',
      '../../src/v8.h',
      '../../src/v8threads.cc',
      '../../src/v8threads.h',
      '../../src/variables.cc',
      '../../src/variables.h',
      '../../src/version.cc',
      '../../src/version.h',
      '../../src/virtual-frame.h',
      '../../src/virtual-frame.cc',
      '../../src/zone-inl.h',
      '../../src/zone.cc',
      '../../src/zone.h',
    ],
    'not_base_source_files': [
      # These files are #included by others and are not meant to be compiled
      # directly.
      '../../src/third_party/dtoa/dtoa.c',
    ],
    'd8_source_files': [
      '../../src/d8-debug.cc',
      '../../src/d8-posix.cc',
      '../../src/d8-readline.cc',
      '../../src/d8-windows.cc',
      '../../src/d8.cc',
    ],
  },
  'includes': [
    '../../../build/common.gypi',
  ],
  'target_defaults': {
    'defines': [
      'ENABLE_LOGGING_AND_PROFILING',
    ],
    'configurations': {
      'Debug': {
        'defines': [
          'DEBUG',
          '_DEBUG',
          'ENABLE_DISASSEMBLER',
        ],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'Optimizations': '0',
            'RuntimeLibrary': '1',
          },
          'VCLinkerTool': {
            'LinkIncremental': '2',
          },
        },
      },
      'Release': {
        'conditions': [
          ['OS=="linux"', {
            'cflags!': [
              '-O2',
            ],
            'cflags': [
              '-fomit-frame-pointer',
              '-O3',
            ],
            'cflags_cc': [
              '-fno-rtti',
            ],
          }],
          ['OS=="win"', {
            'msvs_configuration_attributes': {
              'OutputDirectory': '$(SolutionDir)$(ConfigurationName)',
              'IntermediateDirectory': '$(OutDir)\\obj\\$(ProjectName)',
              'CharacterSet': '1',
            },
            'msvs_settings': {
              'VCCLCompilerTool': {
                'RuntimeLibrary': '0',
                'Optimizations': '2',
                'InlineFunctionExpansion': '2',
                'EnableIntrinsicFunctions': 'true',
                'FavorSizeOrSpeed': '0',
                'OmitFramePointers': 'true',
                'StringPooling': 'true',
              },
              'VCLinkerTool': {
                'LinkIncremental': '1',
                'OptimizeReferences': '2',
                'OptimizeForWindows98': '1',
                'EnableCOMDATFolding': '2',
              },
            },
          }],
        ],
      },
    },
    'xcode_settings': {
      'GCC_ENABLE_CPP_EXCEPTIONS': 'NO',
      'GCC_ENABLE_CPP_RTTI': 'NO',
    },
  },
  'targets': [
    # Targets that apply to any architecture.
    {
      'target_name': 'js2c',
      'type': 'none',
      'variables': {
        'library_files': [
          '../../src/runtime.js',
          '../../src/v8natives.js',
          '../../src/array.js',
          '../../src/string.js',
          '../../src/uri.js',
          '../../src/math.js',
          '../../src/messages.js',
          '../../src/apinatives.js',
          '../../src/debug-delay.js',
          '../../src/mirror-delay.js',
          '../../src/date-delay.js',
          '../../src/json-delay.js',
          '../../src/regexp-delay.js',
          '../../src/macros.py',
        ],
      },
      'actions': [
        {
          'action_name': 'js2c',
          'inputs': [
            '../../tools/js2c.py',
            '<@(library_files)',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/libraries.cc',
            '<(SHARED_INTERMEDIATE_DIR)/libraries-empty.cc',
          ],
          'action': ['python', '../../tools/js2c.py', '<@(_outputs)', 'CORE', '<@(library_files)'],
        },
      ],
    },
    {
      'target_name': 'd8_js2c',
      'type': 'none',
      'variables': {
        'library_files': [
          '../../src/d8.js',
          '../../src/macros.py',
        ],
      },
      'actions': [
        {
          'action_name': 'js2c',
          'inputs': [
            '../../tools/js2c.py',
            '<@(library_files)',
          ],
          'extra_inputs': [
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/d8-js.cc',
            '<(SHARED_INTERMEDIATE_DIR)/d8-js-empty.cc',
          ],
          'action': ['python', '../../tools/js2c.py', '<@(_outputs)', 'D8', '<@(library_files)'],
        },
      ],
    },

    # Targets to build v8 for the native architecture (ia32).
    {
      'target_name': 'v8_base',
      'type': '<(library)',
      'defines': [
        'V8_TARGET_ARCH_IA32'
      ],
      'include_dirs+': [
        '../../src',
        '../../src/ia32',
      ],
      'msvs_guid': 'EC8B7909-62AF-470D-A75D-E1D89C837142',
      'sources': [
        '<@(base_source_files)',
      ],
      'sources!': [
        '<@(not_base_source_files)',
      ],
      'sources/': [
        ['exclude', '-arm\\.cc$'],
        ['exclude', 'src/platform-.*\\.cc$' ],
      ],
      'conditions': [
        ['OS=="linux"',
          {
            'link_settings': {
              'libraries': [
                # Needed for clock_gettime() used by src/platform-linux.cc.
                '-lrt',
              ],
            },
            'sources/': [
              ['include', 'src/platform-linux\\.cc$'],
              ['include', 'src/platform-posix\\.cc$']
            ]
          }
        ],
        ['OS=="mac"',
          {
            'sources/': [
              ['include', 'src/platform-macos\\.cc$'],
              ['include', 'src/platform-posix\\.cc$']
            ]
          }
        ],
        ['OS=="win"', {
          'sources/': [['include', 'src/platform-win32\\.cc$']],
          # 4355, 4800 came from common.vsprops
          # 4018, 4244 were a per file config on dtoa-config.c
          # TODO: It's probably possible and desirable to stop disabling the
          # dtoa-specific warnings by modifying dtoa as was done in Chromium
          # r9255.  Refer to that revision for details.
          'msvs_disabled_warnings': [4355, 4800, 4018, 4244],
          'link_settings':  {
            'libraries': [ '-lwinmm.lib' ],
          },
        }],
      ],
    },
    {
      'target_name': 'v8_nosnapshot',
      'type': '<(library)',
      'defines': [
        'V8_TARGET_ARCH_IA32'
      ],
      'dependencies': [
        'js2c',
        'v8_base',
      ],
      'include_dirs': [
        '../../src',
      ],
      'sources': [
        '<(SHARED_INTERMEDIATE_DIR)/libraries.cc',
        '../../src/snapshot-empty.cc',
      ],
      'export_dependent_settings': [
        'v8_base',
      ],
    },
    {
      'target_name': 'mksnapshot',
      'type': 'executable',
      'dependencies': [
        'v8_nosnapshot',
      ],
      'msvs_guid': '865575D0-37E2-405E-8CBA-5F6C485B5A26',
      'sources': [
        '../../src/mksnapshot.cc',
      ],
    },
    {
      'target_name': 'v8',
      'type': '<(library)',
      'defines': [
        'V8_TARGET_ARCH_IA32'
      ],
      'dependencies': [
        'js2c',
        'mksnapshot',
        'v8_base',
      ],
      'msvs_guid': '21E22961-22BF-4493-BD3A-868F93DA5179',
      'actions': [
        {
          'action_name': 'mksnapshot',
          'inputs': [
            '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)mksnapshot<(EXECUTABLE_SUFFIX)',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/snapshot.cc',
          ],
          'action': ['<@(_inputs)', '<@(_outputs)'],
        },
      ],
      'include_dirs': [
        '../../src',
      ],
      'sources': [
        '<(SHARED_INTERMEDIATE_DIR)/libraries-empty.cc',
        '<(INTERMEDIATE_DIR)/snapshot.cc',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../../include',
        ],
      },
      'export_dependent_settings': [
        'v8_base',
      ],
    },
    {
      'target_name': 'v8_shell',
      'type': 'executable',
      'defines': [
        'V8_TARGET_ARCH_IA32'
      ],
      'dependencies': [
        'v8',
      ],
      'sources': [
        '../../samples/shell.cc',
      ],
      'conditions': [
        [ 'OS=="win"', {
          # This could be gotten by not setting chromium_code, if that's OK.
          'defines': ['_CRT_SECURE_NO_WARNINGS'],
        }],
      ],
    },
  ],

  'conditions': [ ['OS=="mac"', { 'targets': [
    # TODO(bradnelson):  temporarily disable 'd8' target on Windows while
    # we work fix the performance regressions.
    # TODO(sgk):  temporarily disable 'd8' target on Linux while
    # we work out getting the readline library on all the systems.
    {
      'target_name': 'd8',
      'type': 'executable',
      'dependencies': [
        'd8_js2c',
        'v8',
      ],
      'defines': [
        'V8_TARGET_ARCH_IA32'
      ],
      'include_dirs': [
        '../../src',
      ],
      'sources': [
        '<(SHARED_INTERMEDIATE_DIR)/d8-js.cc',
        '<@(d8_source_files)',
      ],
      'conditions': [
        [ 'OS=="linux"', {
          'sources!': [ '../../src/d8-windows.cc' ],
          'link_settings': { 'libraries': [ '-lreadline' ] },
        }],
        [ 'OS=="mac"', {
          'sources!': [ '../../src/d8-windows.cc' ],
          'link_settings': { 'libraries': [
            '$(SDKROOT)/usr/lib/libreadline.dylib'
          ]},
        }],
        [ 'OS=="win"', {
          'sources!': [ '../../src/d8-readline.cc', '../../src/d8-posix.cc' ],
        }],
      ],
    },
    # TODO(sgk):  temporarily disable the arm targets on Linux while
    # we work out how to refactor the generator and/or add configuration
    # settings to the .gyp file to handle building both variants in
    # the same output directory.
    #
    # ARM targets, to test ARM code generation.  These use an ARM simulator
    # (src/simulator-arm.cc).  The ARM targets are not snapshot-enabled.
    {
      'target_name': 'v8_arm',
      'type': '<(library)',
      'dependencies': [
        'js2c',
      ],
      'defines': [
        'V8_TARGET_ARCH_ARM',
      ],
      'include_dirs+': [
        '../../src',
        '../../src/arm',
      ],
      'sources': [
        '<@(base_source_files)',
        '<(SHARED_INTERMEDIATE_DIR)/libraries.cc',
        '../../src/snapshot-empty.cc',
      ],
      'sources!': [
        '<@(not_base_source_files)',
      ],
      'sources/': [
        ['exclude', '-ia32\\.cc$'],
        ['exclude', 'src/platform-.*\\.cc$' ],
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../../include',
        ],
      },
      'conditions': [
        ['OS=="linux"',
          {
            'sources/': [
              ['include', 'src/platform-linux\\.cc$'],
              ['include', 'src/platform-posix\\.cc$']
            ]
          }
        ],
        ['OS=="mac"',
          {
            'sources/': [
              ['include', 'src/platform-macos\\.cc$'],
              ['include', 'src/platform-posix\\.cc$']
            ]
          }
        ],
        ['OS=="win"', {
          'sources/': [['include', 'src/platform-win32\\.cc$']],
          # 4355, 4800 came from common.vsprops
          # 4018, 4244 were a per file config on dtoa-config.c
          # TODO: It's probably possible and desirable to stop disabling the
          # dtoa-specific warnings by modifying dtoa as was done in Chromium
          # r9255.  Refer to that revision for details.
          'msvs_disabled_warnings': [4355, 4800, 4018, 4244],
        }],
      ],
    },
    {
      'target_name': 'v8_shell_arm',
      'type': 'executable',
      'dependencies': [
        'v8_arm',
      ],
      'defines': [
        'V8_TARGET_ARCH_ARM',
      ],
      'sources': [
        '../../samples/shell.cc',
      ],
      'conditions': [
        [ 'OS=="win"', {
          # This could be gotten by not setting chromium_code, if that's OK.
          'defines': ['_CRT_SECURE_NO_WARNINGS'],
        }],
      ],
    },
    {
      'target_name': 'd8_arm',
      'type': 'executable',
      'dependencies': [
        'd8_js2c',
        'v8_arm',
      ],
      'defines': [
        'V8_TARGET_ARCH_ARM',
      ],
      'include_dirs': [
        '../../src',
      ],
      'sources': [
        '<(SHARED_INTERMEDIATE_DIR)/d8-js.cc',
        '<@(d8_source_files)',
      ],
      'conditions': [
        [ 'OS=="linux"', {
          'sources!': [ '../../src/d8-windows.cc' ],
          'link_settings': { 'libraries': [ '-lreadline' ] },
        }],
        [ 'OS=="mac"', {
          'sources!': [ '../../src/d8-windows.cc' ],
          'link_settings': { 'libraries': [
            '$(SDKROOT)/usr/lib/libreadline.dylib'
          ]},
        }],
        [ 'OS=="win"', {
          'sources!': [ '../../src/d8-readline.cc', '../../src/d8-posix.cc' ],
        }],
      ],
    },
  ]}], # OS != "linux" (temporary, TODO(sgk))


    ['OS=="win"', {
      'target_defaults': {
        'defines': [
          '_USE_32BIT_TIME_T',
          '_CRT_SECURE_NO_DEPRECATE',
          '_CRT_NONSTDC_NO_DEPRECATE',
        ],
        'msvs_settings': {
          'VCLinkerTool': {
            'AdditionalOptions': '/IGNORE:4221 /NXCOMPAT',
          },
        },
      },
    }],
  ],
}
