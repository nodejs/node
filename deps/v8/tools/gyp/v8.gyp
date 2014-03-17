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

{
  'variables': {
    'v8_code': 1,
    'v8_random_seed%': 314159265,
  },
  'includes': ['../../build/toolchain.gypi', '../../build/features.gypi'],
  'targets': [
    {
      'target_name': 'v8',
      'dependencies_traverse': 1,
      'conditions': [
        ['want_separate_host_toolset==1', {
          'toolsets': ['host', 'target'],
        }, {
          'toolsets': ['target'],
        }],
        ['v8_use_snapshot=="true"', {
          # The dependency on v8_base should come from a transitive
          # dependency however the Android toolchain requires libv8_base.a
          # to appear before libv8_snapshot.a so it's listed explicitly.
          'dependencies': ['v8_base.<(v8_target_arch)', 'v8_snapshot'],
        },
        {
          # The dependency on v8_base should come from a transitive
          # dependency however the Android toolchain requires libv8_base.a
          # to appear before libv8_snapshot.a so it's listed explicitly.
          'dependencies': [
            'v8_base.<(v8_target_arch)',
            'v8_nosnapshot.<(v8_target_arch)',
          ],
        }],
        ['component=="shared_library"', {
          'type': '<(component)',
          'sources': [
            # Note: on non-Windows we still build this file so that gyp
            # has some sources to link into the component.
            '../../src/v8dll-main.cc',
          ],
          'defines': [
            'V8_SHARED',
            'BUILDING_V8_SHARED',
          ],
          'direct_dependent_settings': {
            'defines': [
              'V8_SHARED',
              'USING_V8_SHARED',
            ],
          },
          'target_conditions': [
            ['OS=="android" and _toolset=="target"', {
              'libraries': [
                '-llog',
              ],
              'include_dirs': [
                'src/common/android/include',
              ],
            }],
          ],
          'conditions': [
            ['OS=="mac"', {
              'xcode_settings': {
                'OTHER_LDFLAGS': ['-dynamiclib', '-all_load']
              },
            }],
            ['soname_version!=""', {
              'product_extension': 'so.<(soname_version)',
            }],
          ],
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
      'target_name': 'v8_snapshot',
      'type': 'static_library',
      'conditions': [
        ['want_separate_host_toolset==1', {
          'toolsets': ['host', 'target'],
          'dependencies': [
            'mksnapshot.<(v8_target_arch)#host',
            'js2c#host',
            'generate_trig_table#host',
          ],
        }, {
          'toolsets': ['target'],
          'dependencies': [
            'mksnapshot.<(v8_target_arch)',
            'js2c',
            'generate_trig_table',
          ],
        }],
        ['component=="shared_library"', {
          'defines': [
            'V8_SHARED',
            'BUILDING_V8_SHARED',
          ],
          'direct_dependent_settings': {
            'defines': [
              'V8_SHARED',
              'USING_V8_SHARED',
            ],
          },
        }],
      ],
      'dependencies': [
        'v8_base.<(v8_target_arch)',
      ],
      'include_dirs+': [
        '../../src',
      ],
      'sources': [
        '<(SHARED_INTERMEDIATE_DIR)/libraries.cc',
        '<(SHARED_INTERMEDIATE_DIR)/experimental-libraries.cc',
        '<(SHARED_INTERMEDIATE_DIR)/trig-table.cc',
        '<(INTERMEDIATE_DIR)/snapshot.cc',
      ],
      'actions': [
        {
          'action_name': 'run_mksnapshot',
          'inputs': [
            '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)mksnapshot.<(v8_target_arch)<(EXECUTABLE_SUFFIX)',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/snapshot.cc',
          ],
          'variables': {
            'mksnapshot_flags': [
              '--log-snapshot-positions',
              '--logfile', '<(INTERMEDIATE_DIR)/snapshot.log',
            ],
            'conditions': [
              ['v8_random_seed!=0', {
                'mksnapshot_flags': ['--random-seed', '<(v8_random_seed)'],
              }],
            ],
          },
          'action': [
            '<@(_inputs)',
            '<@(mksnapshot_flags)',
            '<@(_outputs)'
          ],
        },
      ],
    },
    {
      'target_name': 'v8_nosnapshot.<(v8_target_arch)',
      'type': 'static_library',
      'dependencies': [
        'v8_base.<(v8_target_arch)',
      ],
      'include_dirs+': [
        '../../src',
      ],
      'sources': [
        '<(SHARED_INTERMEDIATE_DIR)/libraries.cc',
        '<(SHARED_INTERMEDIATE_DIR)/experimental-libraries.cc',
        '<(SHARED_INTERMEDIATE_DIR)/trig-table.cc',
        '../../src/snapshot-empty.cc',
      ],
      'conditions': [
        ['want_separate_host_toolset==1', {
          'toolsets': ['host', 'target'],
          'dependencies': ['js2c#host', 'generate_trig_table#host'],
        }, {
          'toolsets': ['target'],
          'dependencies': ['js2c', 'generate_trig_table'],
        }],
        ['component=="shared_library"', {
          'defines': [
            'BUILDING_V8_SHARED',
            'V8_SHARED',
          ],
        }],
      ]
    },
    { 'target_name': 'generate_trig_table',
      'type': 'none',
      'conditions': [
        ['want_separate_host_toolset==1', {
          'toolsets': ['host'],
        }, {
          'toolsets': ['target'],
        }],
      ],
      'actions': [
        {
          'action_name': 'generate',
          'inputs': [
            '../../tools/generate-trig-table.py',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/trig-table.cc',
          ],
          'action': [
            'python',
            '../../tools/generate-trig-table.py',
            '<@(_outputs)',
          ],
        },
      ]
    },
    {
      'target_name': 'v8_base.<(v8_target_arch)',
      'type': 'static_library',
      'variables': {
        'optimize': 'max',
      },
      'include_dirs+': [
        '../../src',
      ],
      'sources': [  ### gcmole(all) ###
        '../../src/accessors.cc',
        '../../src/accessors.h',
        '../../src/allocation.cc',
        '../../src/allocation.h',
        '../../src/allocation-site-scopes.cc',
        '../../src/allocation-site-scopes.h',
        '../../src/allocation-tracker.cc',
        '../../src/allocation-tracker.h',
        '../../src/api.cc',
        '../../src/api.h',
        '../../src/arguments.cc',
        '../../src/arguments.h',
        '../../src/assembler.cc',
        '../../src/assembler.h',
        '../../src/assert-scope.h',
        '../../src/ast.cc',
        '../../src/ast.h',
        '../../src/atomicops.h',
        '../../src/atomicops_internals_x86_gcc.cc',
        '../../src/bignum-dtoa.cc',
        '../../src/bignum-dtoa.h',
        '../../src/bignum.cc',
        '../../src/bignum.h',
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
        '../../src/circular-queue.h',
        '../../src/code-stubs.cc',
        '../../src/code-stubs.h',
        '../../src/code-stubs-hydrogen.cc',
        '../../src/code.h',
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
        '../../src/cpu-profiler-inl.h',
        '../../src/cpu-profiler.cc',
        '../../src/cpu-profiler.h',
        '../../src/cpu.cc',
        '../../src/cpu.h',
        '../../src/data-flow.cc',
        '../../src/data-flow.h',
        '../../src/date.cc',
        '../../src/date.h',
        '../../src/dateparser-inl.h',
        '../../src/dateparser.cc',
        '../../src/dateparser.h',
        '../../src/debug-agent.cc',
        '../../src/debug-agent.h',
        '../../src/debug.cc',
        '../../src/debug.h',
        '../../src/deoptimizer.cc',
        '../../src/deoptimizer.h',
        '../../src/disasm.h',
        '../../src/disassembler.cc',
        '../../src/disassembler.h',
        '../../src/diy-fp.cc',
        '../../src/diy-fp.h',
        '../../src/double.h',
        '../../src/dtoa.cc',
        '../../src/dtoa.h',
        '../../src/effects.h',
        '../../src/elements-kind.cc',
        '../../src/elements-kind.h',
        '../../src/elements.cc',
        '../../src/elements.h',
        '../../src/execution.cc',
        '../../src/execution.h',
        '../../src/extensions/externalize-string-extension.cc',
        '../../src/extensions/externalize-string-extension.h',
        '../../src/extensions/free-buffer-extension.cc',
        '../../src/extensions/free-buffer-extension.h',
        '../../src/extensions/gc-extension.cc',
        '../../src/extensions/gc-extension.h',
        '../../src/extensions/statistics-extension.cc',
        '../../src/extensions/statistics-extension.h',
        '../../src/extensions/trigger-failure-extension.cc',
        '../../src/extensions/trigger-failure-extension.h',
        '../../src/factory.cc',
        '../../src/factory.h',
        '../../src/fast-dtoa.cc',
        '../../src/fast-dtoa.h',
        '../../src/fixed-dtoa.cc',
        '../../src/fixed-dtoa.h',
        '../../src/flag-definitions.h',
        '../../src/flags.cc',
        '../../src/flags.h',
        '../../src/frames-inl.h',
        '../../src/frames.cc',
        '../../src/frames.h',
        '../../src/full-codegen.cc',
        '../../src/full-codegen.h',
        '../../src/func-name-inferrer.cc',
        '../../src/func-name-inferrer.h',
        '../../src/gdb-jit.cc',
        '../../src/gdb-jit.h',
        '../../src/global-handles.cc',
        '../../src/global-handles.h',
        '../../src/globals.h',
        '../../src/handles-inl.h',
        '../../src/handles.cc',
        '../../src/handles.h',
        '../../src/hashmap.h',
        '../../src/heap-inl.h',
        '../../src/heap-profiler.cc',
        '../../src/heap-profiler.h',
        '../../src/heap-snapshot-generator-inl.h',
        '../../src/heap-snapshot-generator.cc',
        '../../src/heap-snapshot-generator.h',
        '../../src/heap.cc',
        '../../src/heap.h',
        '../../src/hydrogen-alias-analysis.h',
        '../../src/hydrogen-bce.cc',
        '../../src/hydrogen-bce.h',
        '../../src/hydrogen-bch.cc',
        '../../src/hydrogen-bch.h',
        '../../src/hydrogen-canonicalize.cc',
        '../../src/hydrogen-canonicalize.h',
        '../../src/hydrogen-check-elimination.cc',
        '../../src/hydrogen-check-elimination.h',
        '../../src/hydrogen-dce.cc',
        '../../src/hydrogen-dce.h',
        '../../src/hydrogen-dehoist.cc',
        '../../src/hydrogen-dehoist.h',
        '../../src/hydrogen-environment-liveness.cc',
        '../../src/hydrogen-environment-liveness.h',
        '../../src/hydrogen-escape-analysis.cc',
        '../../src/hydrogen-escape-analysis.h',
        '../../src/hydrogen-flow-engine.h',
        '../../src/hydrogen-instructions.cc',
        '../../src/hydrogen-instructions.h',
        '../../src/hydrogen.cc',
        '../../src/hydrogen.h',
        '../../src/hydrogen-gvn.cc',
        '../../src/hydrogen-gvn.h',
        '../../src/hydrogen-infer-representation.cc',
        '../../src/hydrogen-infer-representation.h',
        '../../src/hydrogen-infer-types.cc',
        '../../src/hydrogen-infer-types.h',
        '../../src/hydrogen-load-elimination.cc',
        '../../src/hydrogen-load-elimination.h',
        '../../src/hydrogen-mark-deoptimize.cc',
        '../../src/hydrogen-mark-deoptimize.h',
        '../../src/hydrogen-mark-unreachable.cc',
        '../../src/hydrogen-mark-unreachable.h',
        '../../src/hydrogen-minus-zero.cc',
        '../../src/hydrogen-minus-zero.h',
        '../../src/hydrogen-osr.cc',
        '../../src/hydrogen-osr.h',
        '../../src/hydrogen-range-analysis.cc',
        '../../src/hydrogen-range-analysis.h',
        '../../src/hydrogen-redundant-phi.cc',
        '../../src/hydrogen-redundant-phi.h',
        '../../src/hydrogen-removable-simulates.cc',
        '../../src/hydrogen-removable-simulates.h',
        '../../src/hydrogen-representation-changes.cc',
        '../../src/hydrogen-representation-changes.h',
        '../../src/hydrogen-sce.cc',
        '../../src/hydrogen-sce.h',
        '../../src/hydrogen-uint32-analysis.cc',
        '../../src/hydrogen-uint32-analysis.h',
        '../../src/i18n.cc',
        '../../src/i18n.h',
        '../../src/icu_util.cc',
        '../../src/icu_util.h',
        '../../src/ic-inl.h',
        '../../src/ic.cc',
        '../../src/ic.h',
        '../../src/incremental-marking.cc',
        '../../src/incremental-marking.h',
        '../../src/interface.cc',
        '../../src/interface.h',
        '../../src/interpreter-irregexp.cc',
        '../../src/interpreter-irregexp.h',
        '../../src/isolate.cc',
        '../../src/isolate.h',
        '../../src/json-parser.h',
        '../../src/json-stringifier.h',
        '../../src/jsregexp-inl.h',
        '../../src/jsregexp.cc',
        '../../src/jsregexp.h',
        '../../src/lazy-instance.h',
        # TODO(jochen): move libplatform/ files to their own target.
        '../../src/libplatform/default-platform.cc',
        '../../src/libplatform/default-platform.h',
        '../../src/libplatform/task-queue.cc',
        '../../src/libplatform/task-queue.h',
        '../../src/libplatform/worker-thread.cc',
        '../../src/libplatform/worker-thread.h',
        '../../src/list-inl.h',
        '../../src/list.h',
        '../../src/lithium-allocator-inl.h',
        '../../src/lithium-allocator.cc',
        '../../src/lithium-allocator.h',
        '../../src/lithium-codegen.cc',
        '../../src/lithium-codegen.h',
        '../../src/lithium.cc',
        '../../src/lithium.h',
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
        '../../src/messages.cc',
        '../../src/messages.h',
        '../../src/natives.h',
        '../../src/objects-debug.cc',
        '../../src/objects-inl.h',
        '../../src/objects-printer.cc',
        '../../src/objects-visiting.cc',
        '../../src/objects-visiting.h',
        '../../src/objects.cc',
        '../../src/objects.h',
        '../../src/once.cc',
        '../../src/once.h',
        '../../src/optimizing-compiler-thread.h',
        '../../src/optimizing-compiler-thread.cc',
        '../../src/parser.cc',
        '../../src/parser.h',
        '../../src/platform/elapsed-timer.h',
        '../../src/platform/time.cc',
        '../../src/platform/time.h',
        '../../src/platform.h',
        '../../src/platform/condition-variable.cc',
        '../../src/platform/condition-variable.h',
        '../../src/platform/mutex.cc',
        '../../src/platform/mutex.h',
        '../../src/platform/semaphore.cc',
        '../../src/platform/semaphore.h',
        '../../src/platform/socket.cc',
        '../../src/platform/socket.h',
        '../../src/preparse-data-format.h',
        '../../src/preparse-data.cc',
        '../../src/preparse-data.h',
        '../../src/preparser.cc',
        '../../src/preparser.h',
        '../../src/prettyprinter.cc',
        '../../src/prettyprinter.h',
        '../../src/profile-generator-inl.h',
        '../../src/profile-generator.cc',
        '../../src/profile-generator.h',
        '../../src/property-details.h',
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
        '../../src/rewriter.cc',
        '../../src/rewriter.h',
        '../../src/runtime-profiler.cc',
        '../../src/runtime-profiler.h',
        '../../src/runtime.cc',
        '../../src/runtime.h',
        '../../src/safepoint-table.cc',
        '../../src/safepoint-table.h',
        '../../src/sampler.cc',
        '../../src/sampler.h',
        '../../src/scanner-character-streams.cc',
        '../../src/scanner-character-streams.h',
        '../../src/scanner.cc',
        '../../src/scanner.h',
        '../../src/scopeinfo.cc',
        '../../src/scopeinfo.h',
        '../../src/scopes.cc',
        '../../src/scopes.h',
        '../../src/serialize.cc',
        '../../src/serialize.h',
        '../../src/small-pointer-list.h',
        '../../src/smart-pointers.h',
        '../../src/snapshot-common.cc',
        '../../src/snapshot.h',
        '../../src/spaces-inl.h',
        '../../src/spaces.cc',
        '../../src/spaces.h',
        '../../src/store-buffer-inl.h',
        '../../src/store-buffer.cc',
        '../../src/store-buffer.h',
        '../../src/string-search.cc',
        '../../src/string-search.h',
        '../../src/string-stream.cc',
        '../../src/string-stream.h',
        '../../src/strtod.cc',
        '../../src/strtod.h',
        '../../src/stub-cache.cc',
        '../../src/stub-cache.h',
        '../../src/sweeper-thread.h',
        '../../src/sweeper-thread.cc',
        '../../src/token.cc',
        '../../src/token.h',
        '../../src/transitions-inl.h',
        '../../src/transitions.cc',
        '../../src/transitions.h',
        '../../src/type-info.cc',
        '../../src/type-info.h',
        '../../src/types.cc',
        '../../src/types.h',
        '../../src/typing.cc',
        '../../src/typing.h',
        '../../src/unbound-queue-inl.h',
        '../../src/unbound-queue.h',
        '../../src/unicode-inl.h',
        '../../src/unicode.cc',
        '../../src/unicode.h',
        '../../src/unique.h',
        '../../src/uri.h',
        '../../src/utils-inl.h',
        '../../src/utils.cc',
        '../../src/utils.h',
        '../../src/utils/random-number-generator.cc',
        '../../src/utils/random-number-generator.h',
        '../../src/v8-counters.cc',
        '../../src/v8-counters.h',
        '../../src/v8.cc',
        '../../src/v8.h',
        '../../src/v8checks.h',
        '../../src/v8conversions.cc',
        '../../src/v8conversions.h',
        '../../src/v8globals.h',
        '../../src/v8memory.h',
        '../../src/v8threads.cc',
        '../../src/v8threads.h',
        '../../src/v8utils.cc',
        '../../src/v8utils.h',
        '../../src/variables.cc',
        '../../src/variables.h',
        '../../src/version.cc',
        '../../src/version.h',
        '../../src/vm-state-inl.h',
        '../../src/vm-state.h',
        '../../src/zone-inl.h',
        '../../src/zone.cc',
        '../../src/zone.h',
      ],
      'conditions': [
        ['want_separate_host_toolset==1', {
          'toolsets': ['host', 'target'],
        }, {
          'toolsets': ['target'],
        }],
        ['v8_target_arch=="arm"', {
          'sources': [  ### gcmole(arch:arm) ###
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
            '../../src/arm/simulator-arm.cc',
            '../../src/arm/stub-cache-arm.cc',
          ],
        }],
        ['v8_target_arch=="ia32" or v8_target_arch=="mac" or OS=="mac"', {
          'sources': [  ### gcmole(arch:ia32) ###
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
            '../../src/ia32/stub-cache-ia32.cc',
          ],
        }],
        ['v8_target_arch=="mipsel"', {
          'sources': [  ### gcmole(arch:mipsel) ###
            '../../src/mips/assembler-mips.cc',
            '../../src/mips/assembler-mips.h',
            '../../src/mips/assembler-mips-inl.h',
            '../../src/mips/builtins-mips.cc',
            '../../src/mips/codegen-mips.cc',
            '../../src/mips/codegen-mips.h',
            '../../src/mips/code-stubs-mips.cc',
            '../../src/mips/code-stubs-mips.h',
            '../../src/mips/constants-mips.cc',
            '../../src/mips/constants-mips.h',
            '../../src/mips/cpu-mips.cc',
            '../../src/mips/debug-mips.cc',
            '../../src/mips/deoptimizer-mips.cc',
            '../../src/mips/disasm-mips.cc',
            '../../src/mips/frames-mips.cc',
            '../../src/mips/frames-mips.h',
            '../../src/mips/full-codegen-mips.cc',
            '../../src/mips/ic-mips.cc',
            '../../src/mips/lithium-codegen-mips.cc',
            '../../src/mips/lithium-codegen-mips.h',
            '../../src/mips/lithium-gap-resolver-mips.cc',
            '../../src/mips/lithium-gap-resolver-mips.h',
            '../../src/mips/lithium-mips.cc',
            '../../src/mips/lithium-mips.h',
            '../../src/mips/macro-assembler-mips.cc',
            '../../src/mips/macro-assembler-mips.h',
            '../../src/mips/regexp-macro-assembler-mips.cc',
            '../../src/mips/regexp-macro-assembler-mips.h',
            '../../src/mips/simulator-mips.cc',
            '../../src/mips/stub-cache-mips.cc',
          ],
        }],
        ['v8_target_arch=="x64" or v8_target_arch=="mac" or OS=="mac"', {
          'sources': [  ### gcmole(arch:x64) ###
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
            '../../src/x64/stub-cache-x64.cc',
          ],
        }],
        ['OS=="linux"', {
            'link_settings': {
              'conditions': [
                ['v8_compress_startup_data=="bz2"', {
                  'libraries': [
                    '-lbz2',
                  ]
                }],
              ],
              'libraries': [
                '-lrt'
              ]
            },
            'sources': [  ### gcmole(os:linux) ###
              '../../src/platform-linux.cc',
              '../../src/platform-posix.cc'
            ],
          }
        ],
        ['OS=="android"', {
            'defines': [
              'CAN_USE_VFP_INSTRUCTIONS',
            ],
            'sources': [
              '../../src/platform-posix.cc'
            ],
            'conditions': [
              ['host_os=="mac"', {
                'target_conditions': [
                  ['_toolset=="host"', {
                    'sources': [
                      '../../src/platform-macos.cc'
                    ]
                  }, {
                    'sources': [
                      '../../src/platform-linux.cc'
                    ]
                  }],
                ],
              }, {
                # TODO(bmeurer): What we really want here, is this:
                #
                # 'link_settings': {
                #   'target_conditions': [
                #     ['_toolset=="host"', {
                #       'libraries': [
                #         '-lrt'
                #       ]
                #     }]
                #   ]
                # },
                #
                # but we can't do this right now, as the AOSP does not support
                # linking against the host librt, so we need to work around this
                # for now, using the following hack (see platform/time.cc):
                'target_conditions': [
                  ['_toolset=="host"', {
                    'defines': [
                      'V8_LIBRT_NOT_AVAILABLE=1',
                    ],
                  }],
                ],
                'sources': [
                  '../../src/platform-linux.cc'
                ]
              }],
            ],
          },
        ],
        ['OS=="qnx"', {
            'link_settings': {
              'target_conditions': [
                ['_toolset=="host" and host_os=="linux"', {
                  'libraries': [
                    '-lrt'
                  ],
                }],
                ['_toolset=="target"', {
                  'libraries': [
                    '-lbacktrace', '-lsocket'
                  ],
                }],
              ],
            },
            'sources': [
              '../../src/platform-posix.cc',
            ],
            'target_conditions': [
              ['_toolset=="host" and host_os=="linux"', {
                'sources': [
                  '../../src/platform-linux.cc'
                ],
              }],
              ['_toolset=="host" and host_os=="mac"', {
                'sources': [
                  '../../src/platform-macos.cc'
                ],
              }],
              ['_toolset=="target"', {
                'sources': [
                  '../../src/platform-qnx.cc'
                ],
              }],
            ],
          },
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
        ['OS=="netbsd"', {
            'link_settings': {
              'libraries': [
                '-L/usr/pkg/lib -Wl,-R/usr/pkg/lib -lexecinfo',
            ]},
            'sources': [
              '../../src/platform-openbsd.cc',
              '../../src/platform-posix.cc'
            ],
          }
        ],
        ['OS=="solaris"', {
            'link_settings': {
              'libraries': [
                '-lsocket -lnsl',
            ]},
            'sources': [
              '../../src/platform-solaris.cc',
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
          'defines': [
            '_CRT_RAND_S'  # for rand_s()
          ],
          'variables': {
            'gyp_generators': '<!(echo $GYP_GENERATORS)',
          },
          'conditions': [
            ['gyp_generators=="make"', {
              'variables': {
                'build_env': '<!(uname -o)',
              },
              'conditions': [
                ['build_env=="Cygwin"', {
                  'sources': [
                    '../../src/platform-cygwin.cc',
                    '../../src/platform-posix.cc'
                  ],
                }, {
                  'sources': [
                    '../../src/platform-win32.cc',
                    '../../src/win32-math.cc',
                    '../../src/win32-math.h'
                  ],
                }],
              ],
              'link_settings':  {
                'libraries': [ '-lwinmm', '-lws2_32' ],
              },
            }, {
              'sources': [
                '../../src/platform-win32.cc',
                '../../src/win32-math.cc',
                '../../src/win32-math.h'
              ],
              'msvs_disabled_warnings': [4351, 4355, 4800],
              'link_settings':  {
                'libraries': [ '-lwinmm.lib', '-lws2_32.lib' ],
              },
            }],
          ],
        }],
        ['component=="shared_library"', {
          'defines': [
            'BUILDING_V8_SHARED',
            'V8_SHARED',
          ],
        }],
        ['v8_postmortem_support=="true"', {
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/debug-support.cc',
          ]
        }],
        ['v8_enable_i18n_support==1', {
          'dependencies': [
            '<(icu_gyp_path):icui18n',
            '<(icu_gyp_path):icuuc',
          ]
        }, {  # v8_enable_i18n_support==0
          'sources!': [
            '../../src/i18n.cc',
            '../../src/i18n.h',
          ],
        }],
        ['OS=="win" and v8_enable_i18n_support==1', {
          'dependencies': [
            '<(icu_gyp_path):icudata',
          ],
        }],
        ['v8_use_default_platform==0', {
          'sources!': [
            '../../src/default-platform.cc',
            '../../src/default-platform.h',
          ],
        }],
      ],
    },
    {
      'target_name': 'js2c',
      'type': 'none',
      'conditions': [
        ['want_separate_host_toolset==1', {
          'toolsets': ['host'],
        }, {
          'toolsets': ['target'],
        }],
        ['v8_enable_i18n_support==1', {
          'variables': {
            'i18n_library_files': [
              '../../src/i18n.js',
            ],
          },
        }, {
          'variables': {
            'i18n_library_files': [],
          },
        }],
      ],
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
          '../../src/arraybuffer.js',
          '../../src/typedarray.js',
          '../../src/macros.py',
        ],
        'experimental_library_files': [
          '../../src/macros.py',
          '../../src/symbol.js',
          '../../src/proxy.js',
          '../../src/collection.js',
          '../../src/object-observe.js',
          '../../src/promise.js',
          '../../src/generator.js',
          '../../src/array-iterator.js',
          '../../src/harmony-string.js',
          '../../src/harmony-array.js',
          '../../src/harmony-math.js'
        ],
      },
      'actions': [
        {
          'action_name': 'js2c',
          'inputs': [
            '../../tools/js2c.py',
            '<@(library_files)',
            '<@(i18n_library_files)',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/libraries.cc',
          ],
          'action': [
            'python',
            '../../tools/js2c.py',
            '<@(_outputs)',
            'CORE',
            '<(v8_compress_startup_data)',
            '<@(library_files)',
            '<@(i18n_library_files)',
          ],
        },
        {
          'action_name': 'js2c_experimental',
          'inputs': [
            '../../tools/js2c.py',
            '<@(experimental_library_files)',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/experimental-libraries.cc',
          ],
          'action': [
            'python',
            '../../tools/js2c.py',
            '<@(_outputs)',
            'EXPERIMENTAL',
            '<(v8_compress_startup_data)',
            '<@(experimental_library_files)'
          ],
        },
      ],
    },
    {
      'target_name': 'postmortem-metadata',
      'type': 'none',
      'variables': {
        'heapobject_files': [
            '../../src/objects.h',
            '../../src/objects-inl.h',
        ],
      },
      'actions': [
          {
            'action_name': 'gen-postmortem-metadata',
            'inputs': [
              '../../tools/gen-postmortem-metadata.py',
              '<@(heapobject_files)',
            ],
            'outputs': [
              '<(SHARED_INTERMEDIATE_DIR)/debug-support.cc',
            ],
            'action': [
              'python',
              '../../tools/gen-postmortem-metadata.py',
              '<@(_outputs)',
              '<@(heapobject_files)'
            ]
          }
        ]
    },
    {
      'target_name': 'mksnapshot.<(v8_target_arch)',
      'type': 'executable',
      'dependencies': [
        'v8_base.<(v8_target_arch)',
        'v8_nosnapshot.<(v8_target_arch)',
      ],
      'include_dirs+': [
        '../../src',
      ],
      'sources': [
        '../../src/mksnapshot.cc',
      ],
      'conditions': [
        ['want_separate_host_toolset==1', {
          'toolsets': ['host'],
        }, {
          'toolsets': ['target'],
        }],
        ['v8_compress_startup_data=="bz2"', {
          'libraries': [
            '-lbz2',
          ]
        }],
      ],
    },
  ],
}
