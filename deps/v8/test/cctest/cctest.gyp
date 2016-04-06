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
    'generated_file': '<(SHARED_INTERMEDIATE_DIR)/resources.cc',
  },
  'includes': ['../../build/toolchain.gypi', '../../build/features.gypi'],
  'targets': [
    {
      'target_name': 'cctest',
      'type': 'executable',
      'dependencies': [
        'resources',
        '../../tools/gyp/v8.gyp:v8_libplatform',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [  ### gcmole(all) ###
        '<(generated_file)',
        'compiler/c-signature.h',
        'compiler/codegen-tester.cc',
        'compiler/codegen-tester.h',
        'compiler/function-tester.h',
        'compiler/graph-builder-tester.h',
        'compiler/test-basic-block-profiler.cc',
        'compiler/test-branch-combine.cc',
        'compiler/test-changes-lowering.cc',
        'compiler/test-code-stub-assembler.cc',
        'compiler/test-gap-resolver.cc',
        'compiler/test-graph-visualizer.cc',
        'compiler/test-instruction.cc',
        'compiler/test-js-context-specialization.cc',
        'compiler/test-js-constant-cache.cc',
        'compiler/test-js-typed-lowering.cc',
        'compiler/test-jump-threading.cc',
        'compiler/test-linkage.cc',
        'compiler/test-loop-assignment-analysis.cc',
        'compiler/test-loop-analysis.cc',
        'compiler/test-machine-operator-reducer.cc',
        'compiler/test-multiple-return.cc',
        'compiler/test-node.cc',
        'compiler/test-operator.cc',
        'compiler/test-osr.cc',
        'compiler/test-pipeline.cc',
        'compiler/test-representation-change.cc',
        'compiler/test-run-bytecode-graph-builder.cc',
        'compiler/test-run-deopt.cc',
        'compiler/test-run-inlining.cc',
        'compiler/test-run-intrinsics.cc',
        'compiler/test-run-jsbranches.cc',
        'compiler/test-run-jscalls.cc',
        'compiler/test-run-jsexceptions.cc',
        'compiler/test-run-jsobjects.cc',
        'compiler/test-run-jsops.cc',
        'compiler/test-run-machops.cc',
        'compiler/test-run-native-calls.cc',
        'compiler/test-run-stackcheck.cc',
        'compiler/test-run-stubs.cc',
        'compiler/test-run-variables.cc',
        'compiler/test-simplified-lowering.cc',
        'cctest.cc',
        'expression-type-collector.cc',
        'expression-type-collector.h',
        'interpreter/test-bytecode-generator.cc',
        'interpreter/test-interpreter.cc',
        'gay-fixed.cc',
        'gay-precision.cc',
        'gay-shortest.cc',
        'heap/heap-tester.h',
        'heap/test-alloc.cc',
        'heap/test-compaction.cc',
        'heap/test-heap.cc',
        'heap/test-incremental-marking.cc',
        'heap/test-lab.cc',
        'heap/test-mark-compact.cc',
        'heap/test-spaces.cc',
        'heap/utils-inl.h',
        'print-extension.cc',
        'profiler-extension.cc',
        'test-accessors.cc',
        'test-api.cc',
        'test-api.h',
        'test-api-accessors.cc',
        'test-api-interceptors.cc',
        'test-api-fast-accessor-builder.cc',
        'test-array-list.cc',
        'test-ast.cc',
        'test-ast-expression-visitor.cc',
        'test-asm-validator.cc',
        'test-atomicops.cc',
        'test-bignum.cc',
        'test-bignum-dtoa.cc',
        'test-bit-vector.cc',
        'test-circular-queue.cc',
        'test-compiler.cc',
        'test-constantpool.cc',
        'test-conversions.cc',
        'test-cpu-profiler.cc',
        'test-date.cc',
        'test-debug.cc',
        'test-decls.cc',
        'test-deoptimization.cc',
        'test-dictionary.cc',
        'test-diy-fp.cc',
        'test-double.cc',
        'test-dtoa.cc',
        'test-elements-kind.cc',
        'test-fast-dtoa.cc',
        'test-feedback-vector.cc',
        'test-field-type-tracking.cc',
        'test-fixed-dtoa.cc',
        'test-flags.cc',
        'test-func-name-inference.cc',
        'test-gc-tracer.cc',
        'test-global-handles.cc',
        'test-global-object.cc',
        'test-hashing.cc',
        'test-hashmap.cc',
        'test-heap-profiler.cc',
        'test-hydrogen-types.cc',
        'test-identity-map.cc',
        'test-inobject-slack-tracking.cc',
        'test-list.cc',
        'test-liveedit.cc',
        'test-lockers.cc',
        'test-log.cc',
        'test-microtask-delivery.cc',
        'test-mementos.cc',
        'test-object-observe.cc',
        'test-parsing.cc',
        'test-platform.cc',
        'test-profile-generator.cc',
        'test-random-number-generator.cc',
        'test-receiver-check-hidden-prototype.cc',
        'test-regexp.cc',
        'test-reloc-info.cc',
        'test-representation.cc',
        'test-sampler-api.cc',
        'test-serialize.cc',
        'test-simd.cc',
        'test-slots-buffer.cc',
        'test-strings.cc',
        'test-symbols.cc',
        'test-strtod.cc',
        'test-thread-termination.cc',
        'test-threads.cc',
        'test-trace-event.cc',
        'test-transitions.cc',
        'test-typedarrays.cc',
        'test-types.cc',
        'test-typing-reset.cc',
        'test-unbound-queue.cc',
        'test-unboxed-doubles.cc',
        'test-unique.cc',
        'test-unscopables-hidden-prototype.cc',
        'test-utils.cc',
        'test-version.cc',
        'test-weakmaps.cc',
        'test-weaksets.cc',
        'trace-extension.cc',
        'wasm/test-run-wasm.cc',
        'wasm/test-run-wasm-js.cc',
        'wasm/test-run-wasm-module.cc',
        'wasm/test-signatures.h',
        'wasm/wasm-run-utils.h',
      ],
      'conditions': [
        ['v8_target_arch=="ia32"', {
          'sources': [  ### gcmole(arch:ia32) ###
            'test-assembler-ia32.cc',
            'test-code-stubs.cc',
            'test-code-stubs-ia32.cc',
            'test-disasm-ia32.cc',
            'test-macro-assembler-ia32.cc',
            'test-log-stack-tracer.cc'
          ],
        }],
        ['v8_target_arch=="x64"', {
          'sources': [  ### gcmole(arch:x64) ###
            'test-assembler-x64.cc',
            'test-code-stubs.cc',
            'test-code-stubs-x64.cc',
            'test-disasm-x64.cc',
            'test-macro-assembler-x64.cc',
            'test-log-stack-tracer.cc'
          ],
        }],
        ['v8_target_arch=="arm"', {
          'sources': [  ### gcmole(arch:arm) ###
            'test-assembler-arm.cc',
            'test-code-stubs.cc',
            'test-code-stubs-arm.cc',
            'test-disasm-arm.cc',
            'test-macro-assembler-arm.cc'
          ],
        }],
        ['v8_target_arch=="arm64"', {
          'sources': [  ### gcmole(arch:arm64) ###
            'test-utils-arm64.cc',
            'test-assembler-arm64.cc',
            'test-code-stubs.cc',
            'test-code-stubs-arm64.cc',
            'test-disasm-arm64.cc',
            'test-fuzz-arm64.cc',
            'test-javascript-arm64.cc',
            'test-js-arm64-variables.cc'
          ],
        }],
        ['v8_target_arch=="ppc"', {
          'sources': [  ### gcmole(arch:ppc) ###
            'test-assembler-ppc.cc',
            'test-code-stubs.cc',
            'test-disasm-ppc.cc'
          ],
        }],
        ['v8_target_arch=="ppc64"', {
          'sources': [  ### gcmole(arch:ppc64) ###
            'test-assembler-ppc.cc',
            'test-code-stubs.cc',
            'test-disasm-ppc.cc'
          ],
        }],
        ['v8_target_arch=="mipsel"', {
          'sources': [  ### gcmole(arch:mipsel) ###
            'test-assembler-mips.cc',
            'test-code-stubs.cc',
            'test-code-stubs-mips.cc',
            'test-disasm-mips.cc',
            'test-macro-assembler-mips.cc'
          ],
        }],
        ['v8_target_arch=="mips64el"', {
          'sources': [
            'test-assembler-mips64.cc',
            'test-code-stubs.cc',
            'test-code-stubs-mips64.cc',
            'test-disasm-mips64.cc',
            'test-macro-assembler-mips64.cc'
          ],
        }],
        ['v8_target_arch=="x87"', {
          'sources': [  ### gcmole(arch:x87) ###
            'test-assembler-x87.cc',
            'test-code-stubs.cc',
            'test-code-stubs-x87.cc',
            'test-disasm-x87.cc',
            'test-macro-assembler-x87.cc',
            'test-log-stack-tracer.cc'
          ],
        }],
        [ 'OS=="linux" or OS=="qnx"', {
          'sources': [
            'test-platform-linux.cc',
          ],
        }],
        [ 'OS=="win"', {
          'sources': [
            'test-platform-win32.cc',
          ],
          'msvs_settings': {
            'VCCLCompilerTool': {
              # MSVS wants this for gay-{precision,shortest}.cc.
              'AdditionalOptions': ['/bigobj'],
            },
          },
        }],
        ['v8_target_arch=="ppc" or v8_target_arch=="ppc64"', {
          # disable fmadd/fmsub so that expected results match generated code in
          # RunFloat64MulAndFloat64Add1 and friends.
          'cflags': ['-ffp-contract=off'],
        }],
        ['OS=="aix"', {
          'ldflags': [ '-Wl,-bbigtoc' ],
        }],
        ['component=="shared_library"', {
          # cctest can't be built against a shared library, so we need to
          # depend on the underlying static target in that case.
          'dependencies': ['../../tools/gyp/v8.gyp:v8_maybe_snapshot'],
        }, {
          'dependencies': ['../../tools/gyp/v8.gyp:v8'],
        }],
      ],
    },
    {
      'target_name': 'resources',
      'type': 'none',
      'variables': {
        'file_list': [
           '../../tools/splaytree.js',
           '../../tools/codemap.js',
           '../../tools/csvparser.js',
           '../../tools/consarray.js',
           '../../tools/profile.js',
           '../../tools/profile_view.js',
           '../../tools/logreader.js',
           'log-eq-of-logging-and-traversal.js',
        ],
      },
      'actions': [
        {
          'action_name': 'js2c',
          'inputs': [
            '../../tools/js2c.py',
            '<@(file_list)',
          ],
          'outputs': [
            '<(generated_file)',
          ],
          'action': [
            'python',
            '../../tools/js2c.py',
            '<@(_outputs)',
            'TEST',  # type
            '<@(file_list)',
          ],
        }
      ],
    },
  ],
  'conditions': [
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'cctest_run',
          'type': 'none',
          'dependencies': [
            'cctest',
          ],
          'includes': [
            '../../build/isolate.gypi',
          ],
          'sources': [
            'cctest.isolate',
          ],
        },
      ],
    }],
  ],
}
