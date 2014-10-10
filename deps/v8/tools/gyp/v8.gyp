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
    'icu_use_data_file_flag%': 0,
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

        ['v8_use_snapshot=="true" and v8_use_external_startup_data==0', {
          # The dependency on v8_base should come from a transitive
          # dependency however the Android toolchain requires libv8_base.a
          # to appear before libv8_snapshot.a so it's listed explicitly.
          'dependencies': ['v8_base', 'v8_snapshot'],
        }],
        ['v8_use_snapshot!="true" and v8_use_external_startup_data==0', {
          # The dependency on v8_base should come from a transitive
          # dependency however the Android toolchain requires libv8_base.a
          # to appear before libv8_snapshot.a so it's listed explicitly.
          'dependencies': ['v8_base', 'v8_nosnapshot'],
        }],
        ['v8_use_external_startup_data==1 and want_separate_host_toolset==1', {
          'dependencies': ['v8_base', 'v8_external_snapshot'],
          'target_conditions': [
            ['_toolset=="host"', {
              'inputs': [
                '<(PRODUCT_DIR)/snapshot_blob_host.bin',
              ],
            }, {
              'inputs': [
                '<(PRODUCT_DIR)/snapshot_blob.bin',
              ],
            }],
          ],
        }],
        ['v8_use_external_startup_data==1 and want_separate_host_toolset==0', {
          'dependencies': ['v8_base', 'v8_external_snapshot'],
          'inputs': [ '<(PRODUCT_DIR)/snapshot_blob.bin', ],
        }],
        ['component=="shared_library"', {
          'type': '<(component)',
          'sources': [
            # Note: on non-Windows we still build this file so that gyp
            # has some sources to link into the component.
            '../../src/v8dll-main.cc',
          ],
          'include_dirs': [
            '../..',
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
            'mksnapshot#host',
            'js2c#host',
          ],
        }, {
          'toolsets': ['target'],
          'dependencies': [
            'mksnapshot',
            'js2c',
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
        'v8_base',
      ],
      'include_dirs+': [
        '../..',
      ],
      'sources': [
        '<(SHARED_INTERMEDIATE_DIR)/libraries.cc',
        '<(SHARED_INTERMEDIATE_DIR)/experimental-libraries.cc',
        '<(INTERMEDIATE_DIR)/snapshot.cc',
        '../../src/snapshot-common.cc',
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
            '<@(INTERMEDIATE_DIR)/snapshot.cc'
          ],
        },
      ],
    },
    {
      'target_name': 'v8_nosnapshot',
      'type': 'static_library',
      'dependencies': [
        'v8_base',
      ],
      'include_dirs+': [
        '../..',
      ],
      'sources': [
        '<(SHARED_INTERMEDIATE_DIR)/libraries.cc',
        '<(SHARED_INTERMEDIATE_DIR)/experimental-libraries.cc',
        '../../src/snapshot-common.cc',
        '../../src/snapshot-empty.cc',
      ],
      'conditions': [
        ['want_separate_host_toolset==1', {
          'toolsets': ['host', 'target'],
          'dependencies': ['js2c#host'],
        }, {
          'toolsets': ['target'],
          'dependencies': ['js2c'],
        }],
        ['component=="shared_library"', {
          'defines': [
            'BUILDING_V8_SHARED',
            'V8_SHARED',
          ],
        }],
      ]
    },
    {
      'target_name': 'v8_external_snapshot',
      'type': 'static_library',
      'conditions': [
        ['want_separate_host_toolset==1', {
          'toolsets': ['host', 'target'],
          'dependencies': [
            'mksnapshot#host',
            'js2c#host',
            'natives_blob',
        ]}, {
          'toolsets': ['target'],
          'dependencies': [
            'mksnapshot',
            'js2c',
            'natives_blob',
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
        'v8_base',
      ],
      'include_dirs+': [
        '../..',
      ],
      'sources': [
        '../../src/natives-external.cc',
        '../../src/snapshot-external.cc',
      ],
      'actions': [
        {
          'action_name': 'run_mksnapshot (external)',
          'inputs': [
            '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)mksnapshot<(EXECUTABLE_SUFFIX)',
          ],
          'conditions': [
            ['want_separate_host_toolset==1', {
              'target_conditions': [
                ['_toolset=="host"', {
                  'outputs': [
                    '<(INTERMEDIATE_DIR)/snapshot.cc',
                    '<(PRODUCT_DIR)/snapshot_blob_host.bin',
                  ],
                }, {
                  'outputs': [
                    '<(INTERMEDIATE_DIR)/snapshot.cc',
                    '<(PRODUCT_DIR)/snapshot_blob.bin',
                  ],
                }],
              ],
            }, {
              'outputs': [
                '<(INTERMEDIATE_DIR)/snapshot.cc',
                '<(PRODUCT_DIR)/snapshot_blob.bin',
              ],
            }],
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
            '<@(INTERMEDIATE_DIR)/snapshot.cc',
            '--startup_blob', '<(PRODUCT_DIR)/snapshot_blob.bin',
          ],
        },
      ],
    },
    {
      'target_name': 'v8_base',
      'type': 'static_library',
      'dependencies': [
        'v8_libbase',
      ],
      'variables': {
        'optimize': 'max',
      },
      'include_dirs+': [
        '../..',
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
        '../../src/assert-scope.cc',
        '../../src/ast-value-factory.cc',
        '../../src/ast-value-factory.h',
        '../../src/ast.cc',
        '../../src/ast.h',
        '../../src/background-parsing-task.cc',
        '../../src/background-parsing-task.h',
        '../../src/bailout-reason.cc',
        '../../src/bailout-reason.h',
        '../../src/basic-block-profiler.cc',
        '../../src/basic-block-profiler.h',
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
        '../../src/code-factory.cc',
        '../../src/code-factory.h',
        '../../src/code-stubs.cc',
        '../../src/code-stubs.h',
        '../../src/code-stubs-hydrogen.cc',
        '../../src/code.h',
        '../../src/codegen.cc',
        '../../src/codegen.h',
        '../../src/compilation-cache.cc',
        '../../src/compilation-cache.h',
        '../../src/compiler/access-builder.cc',
        '../../src/compiler/access-builder.h',
        '../../src/compiler/ast-graph-builder.cc',
        '../../src/compiler/ast-graph-builder.h',
        '../../src/compiler/basic-block-instrumentor.cc',
        '../../src/compiler/basic-block-instrumentor.h',
        '../../src/compiler/change-lowering.cc',
        '../../src/compiler/change-lowering.h',
        '../../src/compiler/code-generator-impl.h',
        '../../src/compiler/code-generator.cc',
        '../../src/compiler/code-generator.h',
        '../../src/compiler/common-node-cache.h',
        '../../src/compiler/common-operator.cc',
        '../../src/compiler/common-operator.h',
        '../../src/compiler/control-builders.cc',
        '../../src/compiler/control-builders.h',
        '../../src/compiler/frame.h',
        '../../src/compiler/gap-resolver.cc',
        '../../src/compiler/gap-resolver.h',
        '../../src/compiler/generic-algorithm-inl.h',
        '../../src/compiler/generic-algorithm.h',
        '../../src/compiler/generic-graph.h',
        '../../src/compiler/generic-node-inl.h',
        '../../src/compiler/generic-node.h',
        '../../src/compiler/graph-builder.cc',
        '../../src/compiler/graph-builder.h',
        '../../src/compiler/graph-inl.h',
        '../../src/compiler/graph-reducer.cc',
        '../../src/compiler/graph-reducer.h',
        '../../src/compiler/graph-replay.cc',
        '../../src/compiler/graph-replay.h',
        '../../src/compiler/graph-visualizer.cc',
        '../../src/compiler/graph-visualizer.h',
        '../../src/compiler/graph.cc',
        '../../src/compiler/graph.h',
        '../../src/compiler/instruction-codes.h',
        '../../src/compiler/instruction-selector-impl.h',
        '../../src/compiler/instruction-selector.cc',
        '../../src/compiler/instruction-selector.h',
        '../../src/compiler/instruction.cc',
        '../../src/compiler/instruction.h',
        '../../src/compiler/js-builtin-reducer.cc',
        '../../src/compiler/js-builtin-reducer.h',
        '../../src/compiler/js-context-specialization.cc',
        '../../src/compiler/js-context-specialization.h',
        '../../src/compiler/js-generic-lowering.cc',
        '../../src/compiler/js-generic-lowering.h',
        '../../src/compiler/js-graph.cc',
        '../../src/compiler/js-graph.h',
        '../../src/compiler/js-inlining.cc',
        '../../src/compiler/js-inlining.h',
        '../../src/compiler/js-operator.h',
        '../../src/compiler/js-typed-lowering.cc',
        '../../src/compiler/js-typed-lowering.h',
        '../../src/compiler/linkage-impl.h',
        '../../src/compiler/linkage.cc',
        '../../src/compiler/linkage.h',
        '../../src/compiler/machine-operator-reducer.cc',
        '../../src/compiler/machine-operator-reducer.h',
        '../../src/compiler/machine-operator.cc',
        '../../src/compiler/machine-operator.h',
        '../../src/compiler/machine-type.cc',
        '../../src/compiler/machine-type.h',
        '../../src/compiler/node-aux-data-inl.h',
        '../../src/compiler/node-aux-data.h',
        '../../src/compiler/node-cache.cc',
        '../../src/compiler/node-cache.h',
        '../../src/compiler/node-matchers.h',
        '../../src/compiler/node-properties-inl.h',
        '../../src/compiler/node-properties.h',
        '../../src/compiler/node.cc',
        '../../src/compiler/node.h',
        '../../src/compiler/opcodes.h',
        '../../src/compiler/operator-properties-inl.h',
        '../../src/compiler/operator-properties.h',
        '../../src/compiler/operator.cc',
        '../../src/compiler/operator.h',
        '../../src/compiler/phi-reducer.h',
        '../../src/compiler/pipeline.cc',
        '../../src/compiler/pipeline.h',
        '../../src/compiler/raw-machine-assembler.cc',
        '../../src/compiler/raw-machine-assembler.h',
        '../../src/compiler/register-allocator.cc',
        '../../src/compiler/register-allocator.h',
        '../../src/compiler/representation-change.h',
        '../../src/compiler/schedule.cc',
        '../../src/compiler/schedule.h',
        '../../src/compiler/scheduler.cc',
        '../../src/compiler/scheduler.h',
        '../../src/compiler/simplified-lowering.cc',
        '../../src/compiler/simplified-lowering.h',
        '../../src/compiler/simplified-operator-reducer.cc',
        '../../src/compiler/simplified-operator-reducer.h',
        '../../src/compiler/simplified-operator.cc',
        '../../src/compiler/simplified-operator.h',
        '../../src/compiler/source-position.cc',
        '../../src/compiler/source-position.h',
        '../../src/compiler/typer.cc',
        '../../src/compiler/typer.h',
        '../../src/compiler/value-numbering-reducer.cc',
        '../../src/compiler/value-numbering-reducer.h',
        '../../src/compiler/verifier.cc',
        '../../src/compiler/verifier.h',
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
        '../../src/data-flow.cc',
        '../../src/data-flow.h',
        '../../src/date.cc',
        '../../src/date.h',
        '../../src/dateparser-inl.h',
        '../../src/dateparser.cc',
        '../../src/dateparser.h',
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
        '../../src/feedback-slots.h',
        '../../src/field-index.h',
        '../../src/field-index-inl.h',
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
        '../../src/heap-profiler.cc',
        '../../src/heap-profiler.h',
        '../../src/heap-snapshot-generator-inl.h',
        '../../src/heap-snapshot-generator.cc',
        '../../src/heap-snapshot-generator.h',
        '../../src/heap/gc-idle-time-handler.cc',
        '../../src/heap/gc-idle-time-handler.h',
        '../../src/heap/gc-tracer.cc',
        '../../src/heap/gc-tracer.h',
        '../../src/heap/heap-inl.h',
        '../../src/heap/heap.cc',
        '../../src/heap/heap.h',
        '../../src/heap/incremental-marking-inl.h',
        '../../src/heap/incremental-marking.cc',
        '../../src/heap/incremental-marking.h',
        '../../src/heap/mark-compact-inl.h',
        '../../src/heap/mark-compact.cc',
        '../../src/heap/mark-compact.h',
        '../../src/heap/objects-visiting-inl.h',
        '../../src/heap/objects-visiting.cc',
        '../../src/heap/objects-visiting.h',
        '../../src/heap/spaces-inl.h',
        '../../src/heap/spaces.cc',
        '../../src/heap/spaces.h',
        '../../src/heap/store-buffer-inl.h',
        '../../src/heap/store-buffer.cc',
        '../../src/heap/store-buffer.h',
        '../../src/heap/sweeper-thread.h',
        '../../src/heap/sweeper-thread.cc',
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
        '../../src/hydrogen-store-elimination.cc',
        '../../src/hydrogen-store-elimination.h',
        '../../src/hydrogen-types.cc',
        '../../src/hydrogen-types.h',
        '../../src/hydrogen-uint32-analysis.cc',
        '../../src/hydrogen-uint32-analysis.h',
        '../../src/i18n.cc',
        '../../src/i18n.h',
        '../../src/icu_util.cc',
        '../../src/icu_util.h',
        '../../src/ic/access-compiler.cc',
        '../../src/ic/access-compiler.h',
        '../../src/ic/call-optimization.cc',
        '../../src/ic/call-optimization.h',
        '../../src/ic/handler-compiler.cc',
        '../../src/ic/handler-compiler.h',
        '../../src/ic/ic-inl.h',
        '../../src/ic/ic-state.cc',
        '../../src/ic/ic-state.h',
        '../../src/ic/ic.cc',
        '../../src/ic/ic.h',
        '../../src/ic/ic-compiler.cc',
        '../../src/ic/ic-compiler.h',
        '../../src/interface.cc',
        '../../src/interface.h',
        '../../src/interface-descriptors.cc',
        '../../src/interface-descriptors.h',
        '../../src/interpreter-irregexp.cc',
        '../../src/interpreter-irregexp.h',
        '../../src/isolate.cc',
        '../../src/isolate.h',
        '../../src/json-parser.h',
        '../../src/json-stringifier.h',
        '../../src/jsregexp-inl.h',
        '../../src/jsregexp.cc',
        '../../src/jsregexp.h',
        '../../src/list-inl.h',
        '../../src/list.h',
        '../../src/lithium-allocator-inl.h',
        '../../src/lithium-allocator.cc',
        '../../src/lithium-allocator.h',
        '../../src/lithium-codegen.cc',
        '../../src/lithium-codegen.h',
        '../../src/lithium.cc',
        '../../src/lithium.h',
        '../../src/lithium-inl.h',
        '../../src/liveedit.cc',
        '../../src/liveedit.h',
        '../../src/log-inl.h',
        '../../src/log-utils.cc',
        '../../src/log-utils.h',
        '../../src/log.cc',
        '../../src/log.h',
        '../../src/lookup-inl.h',
        '../../src/lookup.cc',
        '../../src/lookup.h',
        '../../src/macro-assembler.h',
        '../../src/messages.cc',
        '../../src/messages.h',
        '../../src/msan.h',
        '../../src/natives.h',
        '../../src/objects-debug.cc',
        '../../src/objects-inl.h',
        '../../src/objects-printer.cc',
        '../../src/objects.cc',
        '../../src/objects.h',
        '../../src/optimizing-compiler-thread.cc',
        '../../src/optimizing-compiler-thread.h',
        '../../src/ostreams.cc',
        '../../src/ostreams.h',
        '../../src/parser.cc',
        '../../src/parser.h',
        '../../src/perf-jit.cc',
        '../../src/perf-jit.h',
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
        '../../src/prototype.h',
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
        '../../src/runtime/runtime-collections.cc',
        '../../src/runtime/runtime-compiler.cc',
        '../../src/runtime/runtime-i18n.cc',
        '../../src/runtime/runtime-json.cc',
        '../../src/runtime/runtime-maths.cc',
        '../../src/runtime/runtime-numbers.cc',
        '../../src/runtime/runtime-regexp.cc',
        '../../src/runtime/runtime-strings.cc',
        '../../src/runtime/runtime-test.cc',
        '../../src/runtime/runtime-typedarray.cc',
        '../../src/runtime/runtime-uri.cc',
        '../../src/runtime/runtime-utils.h',
        '../../src/runtime/runtime.cc',
        '../../src/runtime/runtime.h',
        '../../src/runtime/string-builder.h',
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
        '../../src/snapshot.h',
        '../../src/snapshot-source-sink.cc',
        '../../src/snapshot-source-sink.h',
        '../../src/string-search.cc',
        '../../src/string-search.h',
        '../../src/string-stream.cc',
        '../../src/string-stream.h',
        '../../src/strtod.cc',
        '../../src/strtod.h',
        '../../src/ic/stub-cache.cc',
        '../../src/ic/stub-cache.h',
        '../../src/token.cc',
        '../../src/token.h',
        '../../src/transitions-inl.h',
        '../../src/transitions.cc',
        '../../src/transitions.h',
        '../../src/type-feedback-vector-inl.h',
        '../../src/type-feedback-vector.cc',
        '../../src/type-feedback-vector.h',
        '../../src/type-info.cc',
        '../../src/type-info.h',
        '../../src/types-inl.h',
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
        '../../src/v8.cc',
        '../../src/v8.h',
        '../../src/v8memory.h',
        '../../src/v8threads.cc',
        '../../src/v8threads.h',
        '../../src/variables.cc',
        '../../src/variables.h',
        '../../src/vector.h',
        '../../src/version.cc',
        '../../src/version.h',
        '../../src/vm-state-inl.h',
        '../../src/vm-state.h',
        '../../src/zone-inl.h',
        '../../src/zone.cc',
        '../../src/zone.h',
        '../../third_party/fdlibm/fdlibm.cc',
        '../../third_party/fdlibm/fdlibm.h',
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
            '../../src/arm/interface-descriptors-arm.cc',
            '../../src/arm/interface-descriptors-arm.h',
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
            '../../src/compiler/arm/code-generator-arm.cc',
            '../../src/compiler/arm/instruction-codes-arm.h',
            '../../src/compiler/arm/instruction-selector-arm.cc',
            '../../src/compiler/arm/linkage-arm.cc',
            '../../src/ic/arm/access-compiler-arm.cc',
            '../../src/ic/arm/handler-compiler-arm.cc',
            '../../src/ic/arm/ic-arm.cc',
            '../../src/ic/arm/ic-compiler-arm.cc',
            '../../src/ic/arm/stub-cache-arm.cc',
          ],
        }],
        ['v8_target_arch=="arm64"', {
          'sources': [  ### gcmole(arch:arm64) ###
            '../../src/arm64/assembler-arm64.cc',
            '../../src/arm64/assembler-arm64.h',
            '../../src/arm64/assembler-arm64-inl.h',
            '../../src/arm64/builtins-arm64.cc',
            '../../src/arm64/codegen-arm64.cc',
            '../../src/arm64/codegen-arm64.h',
            '../../src/arm64/code-stubs-arm64.cc',
            '../../src/arm64/code-stubs-arm64.h',
            '../../src/arm64/constants-arm64.h',
            '../../src/arm64/cpu-arm64.cc',
            '../../src/arm64/debug-arm64.cc',
            '../../src/arm64/decoder-arm64.cc',
            '../../src/arm64/decoder-arm64.h',
            '../../src/arm64/decoder-arm64-inl.h',
            '../../src/arm64/delayed-masm-arm64.cc',
            '../../src/arm64/delayed-masm-arm64.h',
            '../../src/arm64/delayed-masm-arm64-inl.h',
            '../../src/arm64/deoptimizer-arm64.cc',
            '../../src/arm64/disasm-arm64.cc',
            '../../src/arm64/disasm-arm64.h',
            '../../src/arm64/frames-arm64.cc',
            '../../src/arm64/frames-arm64.h',
            '../../src/arm64/full-codegen-arm64.cc',
            '../../src/arm64/instructions-arm64.cc',
            '../../src/arm64/instructions-arm64.h',
            '../../src/arm64/instrument-arm64.cc',
            '../../src/arm64/instrument-arm64.h',
            '../../src/arm64/interface-descriptors-arm64.cc',
            '../../src/arm64/interface-descriptors-arm64.h',
            '../../src/arm64/lithium-arm64.cc',
            '../../src/arm64/lithium-arm64.h',
            '../../src/arm64/lithium-codegen-arm64.cc',
            '../../src/arm64/lithium-codegen-arm64.h',
            '../../src/arm64/lithium-gap-resolver-arm64.cc',
            '../../src/arm64/lithium-gap-resolver-arm64.h',
            '../../src/arm64/macro-assembler-arm64.cc',
            '../../src/arm64/macro-assembler-arm64.h',
            '../../src/arm64/macro-assembler-arm64-inl.h',
            '../../src/arm64/regexp-macro-assembler-arm64.cc',
            '../../src/arm64/regexp-macro-assembler-arm64.h',
            '../../src/arm64/simulator-arm64.cc',
            '../../src/arm64/simulator-arm64.h',
            '../../src/arm64/utils-arm64.cc',
            '../../src/arm64/utils-arm64.h',
            '../../src/compiler/arm64/code-generator-arm64.cc',
            '../../src/compiler/arm64/instruction-codes-arm64.h',
            '../../src/compiler/arm64/instruction-selector-arm64.cc',
            '../../src/compiler/arm64/linkage-arm64.cc',
            '../../src/ic/arm64/access-compiler-arm64.cc',
            '../../src/ic/arm64/handler-compiler-arm64.cc',
            '../../src/ic/arm64/ic-arm64.cc',
            '../../src/ic/arm64/ic-compiler-arm64.cc',
            '../../src/ic/arm64/stub-cache-arm64.cc',
          ],
        }],
        ['v8_target_arch=="ia32"', {
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
            '../../src/ia32/interface-descriptors-ia32.cc',
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
            '../../src/compiler/ia32/code-generator-ia32.cc',
            '../../src/compiler/ia32/instruction-codes-ia32.h',
            '../../src/compiler/ia32/instruction-selector-ia32.cc',
            '../../src/compiler/ia32/linkage-ia32.cc',
            '../../src/ic/ia32/access-compiler-ia32.cc',
            '../../src/ic/ia32/handler-compiler-ia32.cc',
            '../../src/ic/ia32/ic-ia32.cc',
            '../../src/ic/ia32/ic-compiler-ia32.cc',
            '../../src/ic/ia32/stub-cache-ia32.cc',
          ],
        }],
        ['v8_target_arch=="x87"', {
          'sources': [  ### gcmole(arch:x87) ###
            '../../src/x87/assembler-x87-inl.h',
            '../../src/x87/assembler-x87.cc',
            '../../src/x87/assembler-x87.h',
            '../../src/x87/builtins-x87.cc',
            '../../src/x87/code-stubs-x87.cc',
            '../../src/x87/code-stubs-x87.h',
            '../../src/x87/codegen-x87.cc',
            '../../src/x87/codegen-x87.h',
            '../../src/x87/cpu-x87.cc',
            '../../src/x87/debug-x87.cc',
            '../../src/x87/deoptimizer-x87.cc',
            '../../src/x87/disasm-x87.cc',
            '../../src/x87/frames-x87.cc',
            '../../src/x87/frames-x87.h',
            '../../src/x87/full-codegen-x87.cc',
            '../../src/x87/interface-descriptors-x87.cc',
            '../../src/x87/lithium-codegen-x87.cc',
            '../../src/x87/lithium-codegen-x87.h',
            '../../src/x87/lithium-gap-resolver-x87.cc',
            '../../src/x87/lithium-gap-resolver-x87.h',
            '../../src/x87/lithium-x87.cc',
            '../../src/x87/lithium-x87.h',
            '../../src/x87/macro-assembler-x87.cc',
            '../../src/x87/macro-assembler-x87.h',
            '../../src/x87/regexp-macro-assembler-x87.cc',
            '../../src/x87/regexp-macro-assembler-x87.h',
            '../../src/ic/x87/access-compiler-x87.cc',
            '../../src/ic/x87/handler-compiler-x87.cc',
            '../../src/ic/x87/ic-x87.cc',
            '../../src/ic/x87/ic-compiler-x87.cc',
            '../../src/ic/x87/stub-cache-x87.cc',
          ],
        }],
        ['v8_target_arch=="mips" or v8_target_arch=="mipsel"', {
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
            '../../src/mips/interface-descriptors-mips.cc',
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
            '../../src/ic/mips/access-compiler-mips.cc',
            '../../src/ic/mips/handler-compiler-mips.cc',
            '../../src/ic/mips/ic-mips.cc',
            '../../src/ic/mips/ic-compiler-mips.cc',
            '../../src/ic/mips/stub-cache-mips.cc',
          ],
        }],
        ['v8_target_arch=="mips64el"', {
          'sources': [  ### gcmole(arch:mips64el) ###
            '../../src/mips64/assembler-mips64.cc',
            '../../src/mips64/assembler-mips64.h',
            '../../src/mips64/assembler-mips64-inl.h',
            '../../src/mips64/builtins-mips64.cc',
            '../../src/mips64/codegen-mips64.cc',
            '../../src/mips64/codegen-mips64.h',
            '../../src/mips64/code-stubs-mips64.cc',
            '../../src/mips64/code-stubs-mips64.h',
            '../../src/mips64/constants-mips64.cc',
            '../../src/mips64/constants-mips64.h',
            '../../src/mips64/cpu-mips64.cc',
            '../../src/mips64/debug-mips64.cc',
            '../../src/mips64/deoptimizer-mips64.cc',
            '../../src/mips64/disasm-mips64.cc',
            '../../src/mips64/frames-mips64.cc',
            '../../src/mips64/frames-mips64.h',
            '../../src/mips64/full-codegen-mips64.cc',
            '../../src/mips64/interface-descriptors-mips64.cc',
            '../../src/mips64/lithium-codegen-mips64.cc',
            '../../src/mips64/lithium-codegen-mips64.h',
            '../../src/mips64/lithium-gap-resolver-mips64.cc',
            '../../src/mips64/lithium-gap-resolver-mips64.h',
            '../../src/mips64/lithium-mips64.cc',
            '../../src/mips64/lithium-mips64.h',
            '../../src/mips64/macro-assembler-mips64.cc',
            '../../src/mips64/macro-assembler-mips64.h',
            '../../src/mips64/regexp-macro-assembler-mips64.cc',
            '../../src/mips64/regexp-macro-assembler-mips64.h',
            '../../src/mips64/simulator-mips64.cc',
            '../../src/ic/mips64/access-compiler-mips64.cc',
            '../../src/ic/mips64/handler-compiler-mips64.cc',
            '../../src/ic/mips64/ic-mips64.cc',
            '../../src/ic/mips64/ic-compiler-mips64.cc',
            '../../src/ic/mips64/stub-cache-mips64.cc',
          ],
        }],
        ['v8_target_arch=="x64" or v8_target_arch=="x32"', {
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
            '../../src/x64/interface-descriptors-x64.cc',
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
            '../../src/compiler/x64/code-generator-x64.cc',
            '../../src/compiler/x64/instruction-codes-x64.h',
            '../../src/compiler/x64/instruction-selector-x64.cc',
            '../../src/compiler/x64/linkage-x64.cc',
            '../../src/ic/x64/access-compiler-x64.cc',
            '../../src/ic/x64/handler-compiler-x64.cc',
            '../../src/ic/x64/ic-x64.cc',
            '../../src/ic/x64/ic-compiler-x64.cc',
            '../../src/ic/x64/stub-cache-x64.cc',
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
            },
          }
        ],
        ['OS=="win"', {
          'variables': {
            'gyp_generators': '<!(echo $GYP_GENERATORS)',
          },
          'msvs_disabled_warnings': [4351, 4355, 4800],
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
        ['icu_use_data_file_flag==1', {
          'defines': ['ICU_UTIL_DATA_IMPL=ICU_UTIL_DATA_FILE'],
        }, { # else icu_use_data_file_flag !=1
          'conditions': [
            ['OS=="win"', {
              'defines': ['ICU_UTIL_DATA_IMPL=ICU_UTIL_DATA_SHARED'],
            }, {
              'defines': ['ICU_UTIL_DATA_IMPL=ICU_UTIL_DATA_STATIC'],
            }],
          ],
        }],
      ],
    },
    {
      'target_name': 'v8_libbase',
      'type': 'static_library',
      'variables': {
        'optimize': 'max',
      },
      'include_dirs+': [
        '../..',
      ],
      'sources': [
        '../../src/base/atomicops.h',
        '../../src/base/atomicops_internals_arm64_gcc.h',
        '../../src/base/atomicops_internals_arm_gcc.h',
        '../../src/base/atomicops_internals_atomicword_compat.h',
        '../../src/base/atomicops_internals_mac.h',
        '../../src/base/atomicops_internals_mips_gcc.h',
        '../../src/base/atomicops_internals_tsan.h',
        '../../src/base/atomicops_internals_x86_gcc.cc',
        '../../src/base/atomicops_internals_x86_gcc.h',
        '../../src/base/atomicops_internals_x86_msvc.h',
        '../../src/base/bits.cc',
        '../../src/base/bits.h',
        '../../src/base/build_config.h',
        '../../src/base/compiler-specific.h',
        '../../src/base/cpu.cc',
        '../../src/base/cpu.h',
        '../../src/base/division-by-constant.cc',
        '../../src/base/division-by-constant.h',
        '../../src/base/flags.h',
        '../../src/base/lazy-instance.h',
        '../../src/base/logging.cc',
        '../../src/base/logging.h',
        '../../src/base/macros.h',
        '../../src/base/once.cc',
        '../../src/base/once.h',
        '../../src/base/platform/elapsed-timer.h',
        '../../src/base/platform/time.cc',
        '../../src/base/platform/time.h',
        '../../src/base/platform/condition-variable.cc',
        '../../src/base/platform/condition-variable.h',
        '../../src/base/platform/mutex.cc',
        '../../src/base/platform/mutex.h',
        '../../src/base/platform/platform.h',
        '../../src/base/platform/semaphore.cc',
        '../../src/base/platform/semaphore.h',
        '../../src/base/safe_conversions.h',
        '../../src/base/safe_conversions_impl.h',
        '../../src/base/safe_math.h',
        '../../src/base/safe_math_impl.h',
        '../../src/base/sys-info.cc',
        '../../src/base/sys-info.h',
        '../../src/base/utils/random-number-generator.cc',
        '../../src/base/utils/random-number-generator.h',
      ],
      'conditions': [
        ['want_separate_host_toolset==1', {
          'toolsets': ['host', 'target'],
        }, {
          'toolsets': ['target'],
        }],
        ['OS=="linux"', {
            'link_settings': {
              'libraries': [
                '-lrt'
              ]
            },
            'sources': [
              '../../src/base/platform/platform-linux.cc',
              '../../src/base/platform/platform-posix.cc'
            ],
          }
        ],
        ['OS=="android"', {
            'sources': [
              '../../src/base/platform/platform-posix.cc'
            ],
            'conditions': [
              ['host_os=="mac"', {
                'target_conditions': [
                  ['_toolset=="host"', {
                    'sources': [
                      '../../src/base/platform/platform-macos.cc'
                    ]
                  }, {
                    'sources': [
                      '../../src/base/platform/platform-linux.cc'
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
                  '../../src/base/platform/platform-linux.cc'
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
                    '-lbacktrace'
                  ],
                }],
              ],
            },
            'sources': [
              '../../src/base/platform/platform-posix.cc',
              '../../src/base/qnx-math.h',
            ],
            'target_conditions': [
              ['_toolset=="host" and host_os=="linux"', {
                'sources': [
                  '../../src/base/platform/platform-linux.cc'
                ],
              }],
              ['_toolset=="host" and host_os=="mac"', {
                'sources': [
                  '../../src/base/platform/platform-macos.cc'
                ],
              }],
              ['_toolset=="target"', {
                'sources': [
                  '../../src/base/platform/platform-qnx.cc'
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
              '../../src/base/platform/platform-freebsd.cc',
              '../../src/base/platform/platform-posix.cc'
            ],
          }
        ],
        ['OS=="openbsd"', {
            'link_settings': {
              'libraries': [
                '-L/usr/local/lib -lexecinfo',
            ]},
            'sources': [
              '../../src/base/platform/platform-openbsd.cc',
              '../../src/base/platform/platform-posix.cc'
            ],
          }
        ],
        ['OS=="netbsd"', {
            'link_settings': {
              'libraries': [
                '-L/usr/pkg/lib -Wl,-R/usr/pkg/lib -lexecinfo',
            ]},
            'sources': [
              '../../src/base/platform/platform-openbsd.cc',
              '../../src/base/platform/platform-posix.cc'
            ],
          }
        ],
        ['OS=="solaris"', {
            'link_settings': {
              'libraries': [
                '-lnsl',
            ]},
            'sources': [
              '../../src/base/platform/platform-solaris.cc',
              '../../src/base/platform/platform-posix.cc'
            ],
          }
        ],
        ['OS=="mac"', {
          'sources': [
            '../../src/base/platform/platform-macos.cc',
            '../../src/base/platform/platform-posix.cc'
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
                    '../../src/base/platform/platform-cygwin.cc',
                    '../../src/base/platform/platform-posix.cc'
                  ],
                }, {
                  'sources': [
                    '../../src/base/platform/platform-win32.cc',
                    '../../src/base/win32-headers.h',
                    '../../src/base/win32-math.cc',
                    '../../src/base/win32-math.h'
                  ],
                }],
              ],
              'link_settings':  {
                'libraries': [ '-lwinmm', '-lws2_32' ],
              },
            }, {
              'sources': [
                '../../src/base/platform/platform-win32.cc',
                '../../src/base/win32-headers.h',
                '../../src/base/win32-math.cc',
                '../../src/base/win32-math.h'
              ],
              'msvs_disabled_warnings': [4351, 4355, 4800],
              'link_settings':  {
                'libraries': [ '-lwinmm.lib', '-lws2_32.lib' ],
              },
            }],
          ],
        }],
      ],
    },
    {
      'target_name': 'v8_libplatform',
      'type': 'static_library',
      'variables': {
        'optimize': 'max',
      },
      'dependencies': [
        'v8_libbase',
      ],
      'include_dirs+': [
        '../..',
      ],
      'sources': [
        '../../include/libplatform/libplatform.h',
        '../../src/libplatform/default-platform.cc',
        '../../src/libplatform/default-platform.h',
        '../../src/libplatform/task-queue.cc',
        '../../src/libplatform/task-queue.h',
        '../../src/libplatform/worker-thread.cc',
        '../../src/libplatform/worker-thread.h',
      ],
      'conditions': [
        ['want_separate_host_toolset==1', {
          'toolsets': ['host', 'target'],
        }, {
          'toolsets': ['target'],
        }],
      ],
    },
    {
      'target_name': 'natives_blob',
      'type': 'none',
      'conditions': [
        [ 'v8_use_external_startup_data==1', {
          'conditions': [
            ['want_separate_host_toolset==1', {
              'dependencies': ['js2c#host'],
            }, {
              'dependencies': ['js2c'],
            }],
          ],
          'actions': [{
            'action_name': 'concatenate_natives_blob',
            'inputs': [
              '../../tools/concatenate-files.py',
              '<(SHARED_INTERMEDIATE_DIR)/libraries.bin',
              '<(SHARED_INTERMEDIATE_DIR)/libraries-experimental.bin',
            ],
            'conditions': [
              ['want_separate_host_toolset==1', {
                'target_conditions': [
                  ['_toolset=="host"', {
                    'outputs': [
                      '<(PRODUCT_DIR)/natives_blob_host.bin',
                    ],
                    'action': [
                      'python', '<@(_inputs)', '<(PRODUCT_DIR)/natives_blob_host.bin'
                    ],
                  }, {
                    'outputs': [
                      '<(PRODUCT_DIR)/natives_blob.bin',
                    ],
                    'action': [
                      'python', '<@(_inputs)', '<(PRODUCT_DIR)/natives_blob.bin'
                    ],
                  }],
                ],
              }, {
                'outputs': [
                  '<(PRODUCT_DIR)/natives_blob.bin',
                ],
                'action': [
                  'python', '<@(_inputs)', '<(PRODUCT_DIR)/natives_blob.bin'
                ],
              }],
            ],
          }],
        }],
        ['want_separate_host_toolset==1', {
          'toolsets': ['host', 'target'],
        }, {
          'toolsets': ['target'],
        }],
      ]
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
          '../../src/symbol.js',
          '../../src/array.js',
          '../../src/string.js',
          '../../src/uri.js',
          '../../third_party/fdlibm/fdlibm.js',
          '../../src/math.js',
          '../../src/apinatives.js',
          '../../src/date.js',
          '../../src/regexp.js',
          '../../src/arraybuffer.js',
          '../../src/typedarray.js',
          '../../src/generator.js',
          '../../src/object-observe.js',
          '../../src/collection.js',
          '../../src/weak-collection.js',
          '../../src/collection-iterator.js',
          '../../src/promise.js',
          '../../src/messages.js',
          '../../src/json.js',
          '../../src/array-iterator.js',
          '../../src/string-iterator.js',
          '../../src/debug-debugger.js',
          '../../src/mirror-debugger.js',
          '../../src/liveedit-debugger.js',
          '../../src/macros.py',
        ],
        'experimental_library_files': [
          '../../src/macros.py',
          '../../src/proxy.js',
          '../../src/generator.js',
          '../../src/harmony-string.js',
          '../../src/harmony-array.js',
          '../../src/harmony-classes.js',
        ],
        'libraries_bin_file': '<(SHARED_INTERMEDIATE_DIR)/libraries.bin',
        'libraries_experimental_bin_file': '<(SHARED_INTERMEDIATE_DIR)/libraries-experimental.bin',
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
            '<(SHARED_INTERMEDIATE_DIR)/libraries.cc',
            'CORE',
            '<(v8_compress_startup_data)',
            '<@(library_files)',
            '<@(i18n_library_files)',
          ],
          'conditions': [
            [ 'v8_use_external_startup_data==1', {
              'outputs': ['<@(libraries_bin_file)'],
              'action': [
                '--startup_blob', '<@(libraries_bin_file)',
              ],
            }],
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
            '<(SHARED_INTERMEDIATE_DIR)/experimental-libraries.cc',
            'EXPERIMENTAL',
            '<(v8_compress_startup_data)',
            '<@(experimental_library_files)'
          ],
          'conditions': [
            [ 'v8_use_external_startup_data==1', {
              'outputs': ['<@(libraries_experimental_bin_file)'],
              'action': [
                '--startup_blob', '<@(libraries_experimental_bin_file)'
              ],
            }],
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
      'target_name': 'mksnapshot',
      'type': 'executable',
      'dependencies': ['v8_base', 'v8_nosnapshot', 'v8_libplatform'],
      'include_dirs+': [
        '../..',
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
