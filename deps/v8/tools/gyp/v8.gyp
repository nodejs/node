# Copyright 2011 the V8 project authors. All rights reserved.
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
    'use_system_v8%': 0,
    'msvs_use_common_release': 0,
    'gcc_version%': 'unknown',
    'v8_target_arch%': '<(target_arch)',
    'v8_use_snapshot%': 'true',
    'v8_use_liveobjectlist%': 'false',
  },
  'conditions': [
    ['use_system_v8==0', {
      'target_defaults': {
        'defines': [
          'ENABLE_LOGGING_AND_PROFILING',
          'ENABLE_DEBUGGER_SUPPORT',
          'ENABLE_VMSTATE_TRACKING',
        ],
        'conditions': [
          ['OS!="mac"', {
            # TODO(mark): The OS!="mac" conditional is temporary. It can be
            # removed once the Mac Chromium build stops setting target_arch to
            # ia32 and instead sets it to mac. Other checks in this file for
            # OS=="mac" can be removed at that time as well. This can be cleaned
            # up once http://crbug.com/44205 is fixed.
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
          }],
          ['v8_use_liveobjectlist=="true"', {
            'defines': [
              'ENABLE_DEBUGGER_SUPPORT',
              'INSPECTOR',
              'OBJECT_PRINT',
              'LIVEOBJECTLIST',
            ],
          }],
        ],
        'configurations': {
          'Debug': {
            'defines': [
              'DEBUG',
              '_DEBUG',
              'ENABLE_DISASSEMBLER',
              'V8_ENABLE_CHECKS',
              'OBJECT_PRINT',
            ],
            'msvs_settings': {
              'VCCLCompilerTool': {
                'Optimization': '0',

                'conditions': [
                  ['OS=="win" and component=="shared_library"', {
                    'RuntimeLibrary': '3',  # /MDd
                  }, {
                    'RuntimeLibrary': '1',  # /MTd
                  }],
                ],
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

                  # -fstrict-aliasing.  Mainline gcc
                  # enables this at -O2 and above,
                  # but Apple gcc does not unless it
                  # is specified explicitly.
                  'GCC_STRICT_ALIASING': 'YES',
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
                    'Optimization': '2',
                    'InlineFunctionExpansion': '2',
                    'EnableIntrinsicFunctions': 'true',
                    'FavorSizeOrSpeed': '0',
                    'OmitFramePointers': 'true',
                    'StringPooling': 'true',

                    'conditions': [
                      ['OS=="win" and component=="shared_library"', {
                        'RuntimeLibrary': '2',  #/MD
                      }, {
                        'RuntimeLibrary': '0',  #/MT
                      }],
                    ],
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
          'conditions': [
            ['v8_use_snapshot=="true"', {
              'dependencies': ['v8_snapshot'],
            },
            {
              'dependencies': ['v8_nosnapshot'],
            }],
            ['OS=="win" and component=="shared_library"', {
              'type': '<(component)',
              'sources': [
                '../../src/v8dll-main.cc',
              ],
              'defines': [
                'BUILDING_V8_SHARED'
              ],
              'direct_dependent_settings': {
                'defines': [
                  'USING_V8_SHARED',
                ],
              },
            },
            {
              'type': 'none',
            }],
          ],
          'direct_dependent_settings': {
            'include_dirs': [
               '../../include',
            ],
          },
        },
        {
          'target_name': 'v8_preparser',
          'include_dirs': [
            '../../include',
            '../../src',
          ],
          'sources': [
            '../../src/allocation.cc',
            '../../src/hashmap.cc',
            '../../src/preparse-data.cc',
            '../../src/preparser.cc',
            '../../src/preparser-api.cc',
            '../../src/scanner-base.cc',
            '../../src/token.cc',
            '../../src/unicode.cc',
          ],
          'conditions': [
            ['OS=="win" and component=="shared_library"', {
              'sources': [ '../../src/v8preparserdll-main.cc' ],
              'defines': [ 'BUILDING_V8_SHARED' ],
              'direct_dependent_settings': {
                'defines': [ 'USING_V8_SHARED' ]
              },
              'type': '<(component)',
            } , {
              'type': 'none'
            }],
            ['OS!="win"', {
              'type': '<(library)'
            }],
          ]
        },
        {
          'target_name': 'v8_snapshot',
          'type': '<(library)',
          'conditions': [
            ['OS=="win" and component=="shared_library"', {
              'defines': [
                'BUILDING_V8_SHARED',
              ],
            }],
          ],
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
            }],
            ['OS=="win" and component=="shared_library"', {
              'defines': [
                'BUILDING_V8_SHARED',
              ],
            }],
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
            '../../src/ast-inl.h',
            '../../src/ast.h',
            '../../src/atomicops_internals_x86_gcc.cc',
            '../../src/bignum.cc',
            '../../src/bignum.h',
            '../../src/bignum-dtoa.cc',
            '../../src/bignum-dtoa.h',
            '../../src/bootstrapper.cc',
            '../../src/bootstrapper.h',
            '../../src/builtins.cc',
            '../../src/builtins.h',
            '../../src/bytecodes-irregexp.h',
            '../../src/cached-powers.cc',
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
            '../../src/deoptimizer.cc',
            '../../src/deoptimizer.h',
            '../../src/disasm.h',
            '../../src/disassembler.cc',
            '../../src/disassembler.h',
            '../../src/dtoa.cc',
            '../../src/dtoa.h',
            '../../src/diy-fp.cc',
            '../../src/diy-fp.h',
            '../../src/double.h',
            '../../src/execution.cc',
            '../../src/execution.h',
            '../../src/factory.cc',
            '../../src/factory.h',
            '../../src/fast-dtoa.cc',
            '../../src/fast-dtoa.h',
            '../../src/flag-definitions.h',
            '../../src/fixed-dtoa.cc',
            '../../src/fixed-dtoa.h',
            '../../src/flags.cc',
            '../../src/flags.h',
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
            '../../src/hydrogen.cc',
            '../../src/hydrogen.h',
            '../../src/hydrogen-instructions.cc',
            '../../src/hydrogen-instructions.h',
            '../../src/ic-inl.h',
            '../../src/ic.cc',
            '../../src/ic.h',
            '../../src/inspector.cc',
            '../../src/inspector.h',
            '../../src/interpreter-irregexp.cc',
            '../../src/interpreter-irregexp.h',
            '../../src/jump-target-inl.h',
            '../../src/jump-target.cc',
            '../../src/jump-target.h',
            '../../src/jsregexp.cc',
            '../../src/jsregexp.h',
            '../../src/list-inl.h',
            '../../src/list.h',
            '../../src/lithium.cc',
            '../../src/lithium.h',
            '../../src/lithium-allocator.cc',
            '../../src/lithium-allocator.h',
            '../../src/lithium-allocator-inl.h',
            '../../src/liveedit.cc',
            '../../src/liveedit.h',
            '../../src/liveobjectlist-inl.h',
            '../../src/liveobjectlist.cc',
            '../../src/liveobjectlist.h',
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
            '../../src/objects-printer.cc',
            '../../src/objects-inl.h',
            '../../src/objects-visiting.cc',
            '../../src/objects-visiting.h',
            '../../src/objects.cc',
            '../../src/objects.h',
            '../../src/parser.cc',
            '../../src/parser.h',
            '../../src/platform.h',
            '../../src/preparse-data.cc',
            '../../src/preparse-data.h',
            '../../src/preparser.cc',
            '../../src/preparser.h',
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
            '../../src/runtime-profiler.cc',
            '../../src/runtime-profiler.h',
            '../../src/safepoint-table.cc',
            '../../src/safepoint-table.h',
            '../../src/scanner-base.cc',
            '../../src/scanner-base.h',
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
            '../../src/string-search.cc',
            '../../src/string-search.h',
            '../../src/string-stream.cc',
            '../../src/string-stream.h',
            '../../src/strtod.cc',
            '../../src/strtod.h',
            '../../src/stub-cache.cc',
            '../../src/stub-cache.h',
            '../../src/token.cc',
            '../../src/token.h',
            '../../src/top.cc',
            '../../src/top.h',
            '../../src/type-info.cc',
            '../../src/type-info.h',
            '../../src/unbound-queue-inl.h',
            '../../src/unbound-queue.h',
            '../../src/unicode-inl.h',
            '../../src/unicode.cc',
            '../../src/unicode.h',
            '../../src/utils.cc',
            '../../src/utils.h',
            '../../src/v8-counters.cc',
            '../../src/v8-counters.h',
            '../../src/v8.cc',
            '../../src/v8.h',
            '../../src/v8checks.h',
            '../../src/v8globals.h',
            '../../src/v8threads.cc',
            '../../src/v8threads.h',
            '../../src/v8utils.h',
            '../../src/variables.cc',
            '../../src/variables.h',
            '../../src/version.cc',
            '../../src/version.h',
            '../../src/virtual-frame-inl.h',
            '../../src/virtual-frame.cc',
            '../../src/virtual-frame.h',
            '../../src/vm-state-inl.h',
            '../../src/vm-state.h',
            '../../src/zone-inl.h',
            '../../src/zone.cc',
            '../../src/zone.h',
            '../../src/extensions/externalize-string-extension.cc',
            '../../src/extensions/externalize-string-extension.h',
            '../../src/extensions/gc-extension.cc',
            '../../src/extensions/gc-extension.h',
          ],
          'conditions': [
            ['v8_target_arch=="arm"', {
              'include_dirs+': [
                '../../src/arm',
              ],
              'sources': [
                '../../src/jump-target-light.h',
                '../../src/jump-target-light-inl.h',
                '../../src/jump-target-light.cc',
                '../../src/virtual-frame-light-inl.h',
                '../../src/virtual-frame-light.cc',
                '../../src/arm/assembler-arm-inl.h',
                '../../src/arm/assembler-arm.cc',
                '../../src/arm/assembler-arm.h',
                '../../src/arm/builtins-arm.cc',
                '../../src/arm/code-stubs-arm.cc',
                '../../src/arm/code-stubs-arm.h',
                '../../src/arm/codegen-arm.cc',
                '../../src/arm/codegen-arm.h',
                '../../src/arm/constants-arm.h',
                '../../src/arm/constants-arm.cc',
                '../../src/arm/cpu-arm.cc',
                '../../src/arm/debug-arm.cc',
                '../../src/arm/deoptimizer-arm.cc',
                '../../src/arm/disasm-arm.cc',
                '../../src/arm/frames-arm.cc',
                '../../src/arm/frames-arm.h',
                '../../src/arm/full-codegen-arm.cc',
                '../../src/arm/ic-arm.cc',
                '../../src/arm/jump-target-arm.cc',
                '../../src/arm/lithium-arm.cc',
                '../../src/arm/lithium-arm.h',
                '../../src/arm/lithium-codegen-arm.cc',
                '../../src/arm/lithium-codegen-arm.h',
                '../../src/arm/lithium-gap-resolver-arm.cc',
                '../../src/arm/lithium-gap-resolver-arm.h',
                '../../src/arm/macro-assembler-arm.cc',
                '../../src/arm/macro-assembler-arm.h',
                '../../src/arm/regexp-macro-assembler-arm.cc',
                '../../src/arm/regexp-macro-assembler-arm.h',
                '../../src/arm/register-allocator-arm.cc',
                '../../src/arm/simulator-arm.cc',
                '../../src/arm/stub-cache-arm.cc',
                '../../src/arm/virtual-frame-arm-inl.h',
                '../../src/arm/virtual-frame-arm.cc',
                '../../src/arm/virtual-frame-arm.h',
              ],
              'conditions': [
                # The ARM assembler assumes the host is 32 bits,
                # so force building 32-bit host tools.
                ['host_arch=="x64" and _toolset=="host"', {
                  'cflags': ['-m32'],
                  'ldflags': ['-m32'],
                }]
              ]
            }],
            ['v8_target_arch=="ia32" or v8_target_arch=="mac" or OS=="mac"', {
              'include_dirs+': [
                '../../src/ia32',
              ],
              'sources': [
                '../../src/jump-target-heavy.h',
                '../../src/jump-target-heavy-inl.h',
                '../../src/jump-target-heavy.cc',
                '../../src/virtual-frame-heavy-inl.h',
                '../../src/virtual-frame-heavy.cc',
                '../../src/ia32/assembler-ia32-inl.h',
                '../../src/ia32/assembler-ia32.cc',
                '../../src/ia32/assembler-ia32.h',
                '../../src/ia32/builtins-ia32.cc',
                '../../src/ia32/code-stubs-ia32.cc',
                '../../src/ia32/code-stubs-ia32.h',
                '../../src/ia32/codegen-ia32.cc',
                '../../src/ia32/codegen-ia32.h',
                '../../src/ia32/cpu-ia32.cc',
                '../../src/ia32/debug-ia32.cc',
                '../../src/ia32/deoptimizer-ia32.cc',
                '../../src/ia32/disasm-ia32.cc',
                '../../src/ia32/frames-ia32.cc',
                '../../src/ia32/frames-ia32.h',
                '../../src/ia32/full-codegen-ia32.cc',
                '../../src/ia32/ic-ia32.cc',
                '../../src/ia32/jump-target-ia32.cc',
                '../../src/ia32/lithium-codegen-ia32.cc',
                '../../src/ia32/lithium-codegen-ia32.h',
                '../../src/ia32/lithium-gap-resolver-ia32.cc',
                '../../src/ia32/lithium-gap-resolver-ia32.h',
                '../../src/ia32/lithium-ia32.cc',
                '../../src/ia32/lithium-ia32.h',
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
            ['v8_target_arch=="x64" or v8_target_arch=="mac" or OS=="mac"', {
              'include_dirs+': [
                '../../src/x64',
              ],
              'sources': [
                '../../src/jump-target-heavy.h',
                '../../src/jump-target-heavy-inl.h',
                '../../src/jump-target-heavy.cc',
                '../../src/virtual-frame-heavy-inl.h',
                '../../src/virtual-frame-heavy.cc',
                '../../src/x64/assembler-x64-inl.h',
                '../../src/x64/assembler-x64.cc',
                '../../src/x64/assembler-x64.h',
                '../../src/x64/builtins-x64.cc',
                '../../src/x64/code-stubs-x64.cc',
                '../../src/x64/code-stubs-x64.h',
                '../../src/x64/codegen-x64.cc',
                '../../src/x64/codegen-x64.h',
                '../../src/x64/cpu-x64.cc',
                '../../src/x64/debug-x64.cc',
                '../../src/x64/deoptimizer-x64.cc',
                '../../src/x64/disasm-x64.cc',
                '../../src/x64/frames-x64.cc',
                '../../src/x64/frames-x64.h',
                '../../src/x64/full-codegen-x64.cc',
                '../../src/x64/ic-x64.cc',
                '../../src/x64/jump-target-x64.cc',
                '../../src/x64/lithium-codegen-x64.cc',
                '../../src/x64/lithium-codegen-x64.h',
                '../../src/x64/lithium-gap-resolver-x64.cc',
                '../../src/x64/lithium-gap-resolver-x64.h',
                '../../src/x64/lithium-x64.cc',
                '../../src/x64/lithium-x64.h',
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
              'msvs_disabled_warnings': [4351, 4355, 4800],
              'link_settings':  {
                'libraries': [ '-lwinmm.lib' ],
              },
            }],
            ['OS=="win" and component=="shared_library"', {
              'defines': [
                'BUILDING_V8_SHARED'
              ],
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
            ['OS=="win"', {
              # This could be gotten by not setting chromium_code, if that's OK.
              'defines': ['_CRT_SECURE_NO_WARNINGS'],
            }],
          ],
        },
      ],
    }, { # use_system_v8 != 0
      'targets': [
        {
          'target_name': 'v8',
          'type': 'settings',
          'link_settings': {
            'libraries': [
              '-lv8',
            ],
          },
        },
        {
          'target_name': 'v8_shell',
          'type': 'none',
          'dependencies': [
            'v8'
          ],
        },
      ],
    }],
  ],
}
