# Copyright 2014 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'v8_code': 1,
  },
  'includes': ['../../build/toolchain.gypi', '../../build/features.gypi'],
  'targets': [
    {
      'target_name': 'unittests',
      'type': 'executable',
      'variables': {
        'optimize': 'max',
      },
      'dependencies': [
        '../../testing/gmock.gyp:gmock',
        '../../testing/gtest.gyp:gtest',
        '../../tools/gyp/v8.gyp:v8_libplatform',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [  ### gcmole(all) ###
        'atomic-utils-unittest.cc',
        'base/bits-unittest.cc',
        'base/cpu-unittest.cc',
        'base/division-by-constant-unittest.cc',
        'base/flags-unittest.cc',
        'base/functional-unittest.cc',
        'base/logging-unittest.cc',
        'base/iterator-unittest.cc',
        'base/platform/condition-variable-unittest.cc',
        'base/platform/mutex-unittest.cc',
        'base/platform/platform-unittest.cc',
        'base/platform/semaphore-unittest.cc',
        'base/platform/time-unittest.cc',
        'base/sys-info-unittest.cc',
        'base/utils/random-number-generator-unittest.cc',
        'cancelable-tasks-unittest.cc',
        'char-predicates-unittest.cc',
        'compiler/branch-elimination-unittest.cc',
        'compiler/change-lowering-unittest.cc',
        'compiler/coalesced-live-ranges-unittest.cc',
        'compiler/common-operator-reducer-unittest.cc',
        'compiler/common-operator-unittest.cc',
        'compiler/compiler-test-utils.h',
        'compiler/control-equivalence-unittest.cc',
        'compiler/control-flow-optimizer-unittest.cc',
        'compiler/dead-code-elimination-unittest.cc',
        'compiler/diamond-unittest.cc',
        'compiler/escape-analysis-unittest.cc',
        'compiler/graph-reducer-unittest.cc',
        'compiler/graph-reducer-unittest.h',
        'compiler/graph-trimmer-unittest.cc',
        'compiler/graph-unittest.cc',
        'compiler/graph-unittest.h',
        'compiler/instruction-selector-unittest.cc',
        'compiler/instruction-selector-unittest.h',
        'compiler/instruction-sequence-unittest.cc',
        'compiler/instruction-sequence-unittest.h',
        'compiler/interpreter-assembler-unittest.cc',
        'compiler/interpreter-assembler-unittest.h',
        'compiler/js-builtin-reducer-unittest.cc',
        'compiler/js-context-relaxation-unittest.cc',
        'compiler/js-intrinsic-lowering-unittest.cc',
        'compiler/js-operator-unittest.cc',
        'compiler/js-typed-lowering-unittest.cc',
        'compiler/linkage-tail-call-unittest.cc',
        'compiler/liveness-analyzer-unittest.cc',
        'compiler/live-range-unittest.cc',
        'compiler/load-elimination-unittest.cc',
        'compiler/loop-peeling-unittest.cc',
        'compiler/machine-operator-reducer-unittest.cc',
        'compiler/machine-operator-unittest.cc',
        'compiler/move-optimizer-unittest.cc',
        'compiler/node-cache-unittest.cc',
        'compiler/node-matchers-unittest.cc',
        'compiler/node-properties-unittest.cc',
        'compiler/node-test-utils.cc',
        'compiler/node-test-utils.h',
        'compiler/node-unittest.cc',
        'compiler/opcodes-unittest.cc',
        'compiler/register-allocator-unittest.cc',
        'compiler/schedule-unittest.cc',
        'compiler/select-lowering-unittest.cc',
        'compiler/scheduler-unittest.cc',
        'compiler/simplified-operator-reducer-unittest.cc',
        'compiler/simplified-operator-unittest.cc',
        'compiler/state-values-utils-unittest.cc',
        'compiler/tail-call-optimization-unittest.cc',
        'compiler/typer-unittest.cc',
        'compiler/value-numbering-reducer-unittest.cc',
        'compiler/zone-pool-unittest.cc',
        'counters-unittest.cc',
        'interpreter/bytecodes-unittest.cc',
        'interpreter/bytecode-array-builder-unittest.cc',
        'interpreter/bytecode-array-iterator-unittest.cc',
        'interpreter/bytecode-register-allocator-unittest.cc',
        'interpreter/constant-array-builder-unittest.cc',
        'libplatform/default-platform-unittest.cc',
        'libplatform/task-queue-unittest.cc',
        'libplatform/worker-thread-unittest.cc',
        'heap/bitmap-unittest.cc',
        'heap/gc-idle-time-handler-unittest.cc',
        'heap/memory-reducer-unittest.cc',
        'heap/heap-unittest.cc',
        'heap/scavenge-job-unittest.cc',
        'locked-queue-unittest.cc',
        'run-all-unittests.cc',
        'runtime/runtime-interpreter-unittest.cc',
        'test-utils.h',
        'test-utils.cc',
        'wasm/ast-decoder-unittest.cc',
        'wasm/encoder-unittest.cc',
        'wasm/module-decoder-unittest.cc',
        'wasm/wasm-macro-gen-unittest.cc',
      ],
      'conditions': [
        ['v8_target_arch=="arm"', {
          'sources': [  ### gcmole(arch:arm) ###
            'compiler/arm/instruction-selector-arm-unittest.cc',
          ],
        }],
        ['v8_target_arch=="arm64"', {
          'sources': [  ### gcmole(arch:arm64) ###
            'compiler/arm64/instruction-selector-arm64-unittest.cc',
          ],
        }],
        ['v8_target_arch=="ia32"', {
          'sources': [  ### gcmole(arch:ia32) ###
            'compiler/ia32/instruction-selector-ia32-unittest.cc',
          ],
        }],
        ['v8_target_arch=="mipsel"', {
          'sources': [  ### gcmole(arch:mipsel) ###
            'compiler/mips/instruction-selector-mips-unittest.cc',
          ],
        }],
        ['v8_target_arch=="mips64el"', {
          'sources': [  ### gcmole(arch:mips64el) ###
            'compiler/mips64/instruction-selector-mips64-unittest.cc',
          ],
        }],
        ['v8_target_arch=="x64"', {
          'sources': [  ### gcmole(arch:x64) ###
            'compiler/x64/instruction-selector-x64-unittest.cc',
          ],
        }],
        ['v8_target_arch=="ppc" or v8_target_arch=="ppc64"', {
          'sources': [  ### gcmole(arch:ppc) ###
            'compiler/ppc/instruction-selector-ppc-unittest.cc',
          ],
        }],
        ['OS=="aix"', {
          'ldflags': [ '-Wl,-bbigtoc' ],
        }],
        ['component=="shared_library"', {
          # compiler-unittests can't be built against a shared library, so we
          # need to depend on the underlying static target in that case.
          'dependencies': ['../../tools/gyp/v8.gyp:v8_maybe_snapshot'],
        }, {
          'dependencies': ['../../tools/gyp/v8.gyp:v8'],
        }],
        ['os_posix == 1', {
          # TODO(svenpanne): This is a temporary work-around to fix the warnings
          # that show up because we use -std=gnu++0x instead of -std=c++11.
          'cflags!': [
            '-pedantic',
          ],
          'direct_dependent_settings': {
            'cflags!': [
              '-pedantic',
            ],
          },
        }],
      ],
    },
  ],
  'conditions': [
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'unittests_run',
          'type': 'none',
          'dependencies': [
            'unittests',
          ],
          'includes': [
            '../../build/isolate.gypi',
          ],
          'sources': [
            'unittests.isolate',
          ],
        },
      ],
    }],
  ],
}
