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
    'msvs_use_common_release': 0,
    'gcc_version%': 'unknown',
    'v8_target_arch%': '<(target_arch)',
    'v8_use_snapshot%': 'true',
  },
  'target_defaults': {
    'defines': [
      'ENABLE_LOGGING_AND_PROFILING',
      'ENABLE_DEBUGGER_SUPPORT',
      'ENABLE_VMSTATE_TRACKING',
    ],
    'conditions': [
      ['v8_target_arch=="arm"', {
        'defines': [
          'V8_TARGET_ARCH_ARM',
        ],
      }],
      ['v8_target_arch=="ia32"', {
        'defines': [
          'V8_TARGET_ARCH_IA32',
        ],
      }],
      ['v8_target_arch=="x64"', {
        'defines': [
          'V8_TARGET_ARCH_X64',
        ],
      }],
    ],
    'configurations': {
      'Debug': {
        'defines': [
          'DEBUG',
          '_DEBUG',
          'ENABLE_DISASSEMBLER',
          'V8_ENABLE_CHECKS'
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
        'conditions': [
         ['OS=="freebsd" or OS=="openbsd"', {
           'cflags': [ '-I/usr/local/include' ],
         }],
       ],
      },
      'Release': {
        'conditions': [
          ['OS=="linux" or OS=="freebsd" or OS=="openbsd"', {
            'cflags!': [
              '-O2',
              '-Os',
            ],
            'cflags': [
              '-fomit-frame-pointer',
              '-O3',
            ],
            'conditions': [
              [ 'gcc_version==44', {
                'cflags': [
                  # Avoid gcc 4.4 strict aliasing issues in dtoa.c
                  '-fno-strict-aliasing',
                  # Avoid crashes with gcc 4.4 in the v8 test suite.
                  '-fno-tree-vrp',
                ],
              }],
            ],
          }],
         ['OS=="freebsd" or OS=="openbsd"', {
           'cflags': [ '-I/usr/local/include' ],
         }],
          ['OS=="mac"', {
            'xcode_settings': {
              'GCC_OPTIMIZATION_LEVEL': '3',  # -O3
              'GCC_STRICT_ALIASING': 'YES',   # -fstrict-aliasing.  Mainline gcc
                                              # enables this at -O2 and above,
                                              # but Apple gcc does not unless it
                                              # is specified explicitly.
            },
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
  },
  'targets': [
    {
      'target_name': 'v8',
      'type': 'none',
      'conditions': [
        ['v8_use_snapshot=="true"', {
          'dependencies': ['v8_snapshot'],
        },
        {
          'dependencies': ['v8_nosnapshot'],
        }],
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../../include',
        ],
      },
    },
    {
      'target_name': 'v8_snapshot',
      'type': '<(library)',
      'dependencies': [
        'mksnapshot#host',
        'js2c#host',
        'v8_base',
      ],
      'include_dirs+': [
        '../../src',
      ],
      'sources': [
        '<(SHARED_INTERMEDIATE_DIR)/libraries-empty.cc',
        '<(INTERMEDIATE_DIR)/snapshot.cc',
      ],
      'actions': [
        {
          'action_name': 'run_mksnapshot',
          'inputs': [
            '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)mksnapshot<(EXECUTABLE_SUFFIX)',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/snapshot.cc',
          ],
          'action': ['<@(_inputs)', '<@(_outputs)'],
        },
      ],
    },
    {
      'target_name': 'v8_nosnapshot',
      'type': '<(library)',
      'toolsets': ['host', 'target'],
      'dependencies': [
        'js2c#host',
        'v8_base',
      ],
      'include_dirs+': [
        '../../src',
      ],
      'sources': [
        '<(SHARED_INTERMEDIATE_DIR)/libraries.cc',
        '../../src/snapshot-empty.cc',
      ],
      'conditions': [
        # The ARM assembler assumes the host is 32 bits, so force building
        # 32-bit host tools.
        ['v8_target_arch=="arm" and host_arch=="x64" and _toolset=="host"', {
          'cflags': ['-m32'],
          'ldflags': ['-m32'],
        }]
      ]
    },
    {
      'target_name': 'v8_base',
      'type': '<(library)',
      'toolsets': ['host', 'target'],
      'include_dirs+': [
        '../../src',
      ],
      'sources': [
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
        '../../src/cached-powers.h',
        '../../src/char-predicates-inl.h',
        '../../src/char-predicates.h',
        '../../src/checks.cc',
        '../../src/checks.h',
        '../../src/circular-queue-inl.h',
        '../../src/circular-queue.cc',
        '../../src/circular-queue.h',
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
        '../../src/cpu-profiler-inl.h',
        '../../src/cpu-profiler.cc',
        '../../src/cpu-profiler.h',
        '../../src/data-flow.cc',
        '../../src/data-flow.h',
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
        '../../src/dtoa.cc',
        '../../src/dtoa.h',
        '../../src/dtoa-config.c',
        '../../src/diy-fp.cc',
        '../../src/diy-fp.h',
        '../../src/double.h',
        '../../src/execution.cc',
        '../../src/execution.h',
        '../../src/factory.cc',
        '../../src/factory.h',
        '../../src/fast-codegen.h',
        '../../src/fast-dtoa.cc',
        '../../src/fast-dtoa.h',
        '../../src/flag-definitions.h',
        '../../src/fixed-dtoa.cc',
        '../../src/fixed-dtoa.h',
        '../../src/flags.cc',
        '../../src/flags.h',
        '../../src/flow-graph.cc',
        '../../src/flow-graph.h',
        '../../src/frame-element.cc',
        '../../src/frame-element.h',
        '../../src/frames-inl.h',
        '../../src/frames.cc',
        '../../src/frames.h',
        '../../src/full-codegen.cc',
        '../../src/full-codegen.h',
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
        '../../src/heap-profiler.cc',
        '../../src/heap-profiler.h',
        '../../src/ic-inl.h',
        '../../src/ic.cc',
        '../../src/ic.h',
        '../../src/interpreter-irregexp.cc',
        '../../src/interpreter-irregexp.h',
        '../../src/jump-target-inl.h',
        '../../src/jump-target.cc',
        '../../src/jump-target.h',
        '../../src/jsregexp.cc',
        '../../src/jsregexp.h',
        '../../src/list-inl.h',
        '../../src/list.h',
        '../../src/liveedit.cc',
        '../../src/liveedit.h',
        '../../src/log-inl.h',
        '../../src/log-utils.cc',
        '../../src/log-utils.h',
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
        '../../src/platform.h',
        '../../src/powers-ten.h',
        '../../src/prettyprinter.cc',
        '../../src/prettyprinter.h',
        '../../src/property.cc',
        '../../src/property.h',
        '../../src/profile-generator-inl.h',
        '../../src/profile-generator.cc',
        '../../src/profile-generator.h',
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
        '../../src/type-info.cc',
        '../../src/type-info.h',
        '../../src/unicode-inl.h',
        '../../src/unicode.cc',
        '../../src/unicode.h',
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
        '../../src/virtual-frame-inl.h',
        '../../src/virtual-frame.cc',
        '../../src/virtual-frame.h',
        '../../src/vm-state-inl.h',
        '../../src/vm-state.cc',
        '../../src/vm-state.h',
        '../../src/zone-inl.h',
        '../../src/zone.cc',
        '../../src/zone.h',
      ],
      'conditions': [
        ['v8_target_arch=="arm"', {
          'include_dirs+': [
            '../../src/arm',
          ],
          'sources': [
            '../../src/fast-codegen.cc',
            '../../src/jump-target-light-inl.h',
            '../../src/jump-target-light.cc',
            '../../src/virtual-frame-light-inl.h',
            '../../src/virtual-frame-light.cc',
            '../../src/arm/assembler-arm-inl.h',
            '../../src/arm/assembler-arm.cc',
            '../../src/arm/assembler-arm.h',
            '../../src/arm/builtins-arm.cc',
            '../../src/arm/codegen-arm.cc',
            '../../src/arm/codegen-arm.h',
            '../../src/arm/constants-arm.h',
            '../../src/arm/constants-arm.cc',
            '../../src/arm/cpu-arm.cc',
            '../../src/arm/debug-arm.cc',
            '../../src/arm/disasm-arm.cc',
            '../../src/arm/fast-codegen-arm.cc',
            '../../src/arm/frames-arm.cc',
            '../../src/arm/frames-arm.h',
            '../../src/arm/full-codegen-arm.cc',
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
          ],
          'conditions': [
            # The ARM assembler assumes the host is 32 bits, so force building
            # 32-bit host tools.
            ['host_arch=="x64" and _toolset=="host"', {
              'cflags': ['-m32'],
              'ldflags': ['-m32'],
            }]
          ]
        }],
        ['v8_target_arch=="ia32"', {
          'include_dirs+': [
            '../../src/ia32',
          ],
          'sources': [
            '../../src/jump-target-heavy-inl.h',
            '../../src/jump-target-heavy.cc',
            '../../src/virtual-frame-heavy-inl.h',
            '../../src/virtual-frame-heavy.cc',
            '../../src/ia32/assembler-ia32-inl.h',
            '../../src/ia32/assembler-ia32.cc',
            '../../src/ia32/assembler-ia32.h',
            '../../src/ia32/builtins-ia32.cc',
            '../../src/ia32/codegen-ia32.cc',
            '../../src/ia32/codegen-ia32.h',
            '../../src/ia32/cpu-ia32.cc',
            '../../src/ia32/debug-ia32.cc',
            '../../src/ia32/disasm-ia32.cc',
            '../../src/ia32/fast-codegen-ia32.cc',
            '../../src/ia32/fast-codegen-ia32.h',
            '../../src/ia32/frames-ia32.cc',
            '../../src/ia32/frames-ia32.h',
            '../../src/ia32/full-codegen-ia32.cc',
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
          ],
        }],
        ['v8_target_arch=="x64"', {
          'include_dirs+': [
            '../../src/x64',
          ],
          'sources': [
            '../../src/fast-codegen.cc',
            '../../src/jump-target-heavy-inl.h',
            '../../src/jump-target-heavy.cc',
            '../../src/virtual-frame-heavy-inl.h',
            '../../src/virtual-frame-heavy.cc',
            '../../src/x64/assembler-x64-inl.h',
            '../../src/x64/assembler-x64.cc',
            '../../src/x64/assembler-x64.h',
            '../../src/x64/builtins-x64.cc',
            '../../src/x64/codegen-x64.cc',
            '../../src/x64/codegen-x64.h',
            '../../src/x64/cpu-x64.cc',
            '../../src/x64/debug-x64.cc',
            '../../src/x64/disasm-x64.cc',
            '../../src/x64/fast-codegen-x64.cc',
            '../../src/x64/frames-x64.cc',
            '../../src/x64/frames-x64.h',
            '../../src/x64/full-codegen-x64.cc',
            '../../src/x64/ic-x64.cc',
            '../../src/x64/jump-target-x64.cc',
            '../../src/x64/macro-assembler-x64.cc',
            '../../src/x64/macro-assembler-x64.h',
            '../../src/x64/regexp-macro-assembler-x64.cc',
            '../../src/x64/regexp-macro-assembler-x64.h',
            '../../src/x64/register-allocator-x64.cc',
            '../../src/x64/stub-cache-x64.cc',
            '../../src/x64/virtual-frame-x64.cc',
            '../../src/x64/virtual-frame-x64.h',
          ],
        }],
        ['OS=="linux"', {
            'link_settings': {
              'libraries': [
                # Needed for clock_gettime() used by src/platform-linux.cc.
                '-lrt',
            ]},
            'sources': [
              '../../src/platform-linux.cc',
              '../../src/platform-posix.cc'
            ],
          }
        ],
        ['OS=="freebsd"', {
            'link_settings': {
              'libraries': [
                '-L/usr/local/lib -lexecinfo',
            ]},
            'sources': [
              '../../src/platform-freebsd.cc',
              '../../src/platform-posix.cc'
            ],
          }
        ],
        ['OS=="openbsd"', {
            'link_settings': {
              'libraries': [
                '-L/usr/local/lib -lexecinfo',
            ]},
            'sources': [
              '../../src/platform-openbsd.cc',
              '../../src/platform-posix.cc'
            ],
          }
        ],
        ['OS=="mac"', {
          'sources': [
            '../../src/platform-macos.cc',
            '../../src/platform-posix.cc'
          ]},
        ],
        ['OS=="win"', {
          'sources': [
            '../../src/platform-win32.cc',
          ],
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
      'target_name': 'js2c',
      'type': 'none',
      'toolsets': ['host'],
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
          '../../src/debug-debugger.js',
          '../../src/mirror-debugger.js',
          '../../src/liveedit-debugger.js',
          '../../src/date.js',
          '../../src/json.js',
          '../../src/regexp.js',
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
          'action': [
            'python',
            '../../tools/js2c.py',
            '<@(_outputs)',
            'CORE',
            '<@(library_files)'
          ],
        },
      ],
    },
    {
      'target_name': 'mksnapshot',
      'type': 'executable',
      'toolsets': ['host'],
      'dependencies': [
        'v8_nosnapshot',
      ],
      'include_dirs+': [
        '../../src',
      ],
      'sources': [
        '../../src/mksnapshot.cc',
      ],
      'conditions': [
        # The ARM assembler assumes the host is 32 bits, so force building
        # 32-bit host tools.
        ['v8_target_arch=="arm" and host_arch=="x64" and _toolset=="host"', {
          'cflags': ['-m32'],
          'ldflags': ['-m32'],
        }]
      ]
    },
    {
      'target_name': 'v8_shell',
      'type': 'executable',
      'dependencies': [
        'v8'
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
}
