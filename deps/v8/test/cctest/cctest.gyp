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
      ],
      'include_dirs': [
        '../../src',
      ],
      'sources': [
        '<(generated_file)',
        'cctest.cc',
        'gay-fixed.cc',
        'gay-precision.cc',
        'gay-shortest.cc',
        'print-extension.cc',
        'profiler-extension.cc',
        'test-accessors.cc',
        'test-alloc.cc',
        'test-api.cc',
        'test-ast.cc',
        'test-bignum.cc',
        'test-bignum-dtoa.cc',
        'test-circular-queue.cc',
        'test-compiler.cc',
        'test-condition-variable.cc',
        'test-constantpool.cc',
        'test-conversions.cc',
        'test-cpu.cc',
        'test-cpu-profiler.cc',
        'test-dataflow.cc',
        'test-date.cc',
        'test-debug.cc',
        'test-declarative-accessors.cc',
        'test-decls.cc',
        'test-deoptimization.cc',
        'test-dictionary.cc',
        'test-diy-fp.cc',
        'test-double.cc',
        'test-dtoa.cc',
        'test-fast-dtoa.cc',
        'test-fixed-dtoa.cc',
        'test-flags.cc',
        'test-func-name-inference.cc',
        'test-global-handles.cc',
        'test-global-object.cc',
        'test-hashing.cc',
        'test-hashmap.cc',
        'test-heap.cc',
        'test-heap-profiler.cc',
        'test-libplatform-task-queue.cc',
        'test-libplatform-worker-thread.cc',
        'test-list.cc',
        'test-liveedit.cc',
        'test-lockers.cc',
        'test-log.cc',
        'test-microtask-delivery.cc',
        'test-mark-compact.cc',
        'test-mementos.cc',
        'test-mutex.cc',
        'test-object-observe.cc',
        'test-parsing.cc',
        'test-platform.cc',
        'test-platform-tls.cc',
        'test-profile-generator.cc',
        'test-random-number-generator.cc',
        'test-regexp.cc',
        'test-reloc-info.cc',
        'test-representation.cc',
        'test-semaphore.cc',
        'test-serialize.cc',
        'test-socket.cc',
        'test-spaces.cc',
        'test-strings.cc',
        'test-symbols.cc',
        'test-strtod.cc',
        'test-thread-termination.cc',
        'test-threads.cc',
        'test-time.cc',
        'test-types.cc',
        'test-unbound-queue.cc',
        'test-unique.cc',
        'test-utils.cc',
        'test-version.cc',
        'test-weakmaps.cc',
        'test-weaksets.cc',
        'test-weaktypedarrays.cc',
        'trace-extension.cc'
      ],
      'conditions': [
        ['v8_target_arch=="ia32"', {
          'sources': [
            'test-assembler-ia32.cc',
            'test-code-stubs.cc',
            'test-code-stubs-ia32.cc',
            'test-cpu-ia32.cc',
            'test-disasm-ia32.cc',
            'test-macro-assembler-ia32.cc',
            'test-log-stack-tracer.cc'
          ],
        }],
        ['v8_target_arch=="x64"', {
          'sources': [
            'test-assembler-x64.cc',
            'test-code-stubs.cc',
            'test-code-stubs-x64.cc',
            'test-cpu-x64.cc',
            'test-macro-assembler-x64.cc',
            'test-log-stack-tracer.cc'
          ],
        }],
        ['v8_target_arch=="arm"', {
          'sources': [
            'test-assembler-arm.cc',
            'test-code-stubs.cc',
            'test-code-stubs-arm.cc',
            'test-disasm-arm.cc',
            'test-macro-assembler-arm.cc'
          ],
        }],
        ['v8_target_arch=="a64"', {
          'sources': [
            'test-utils-a64.cc',
            'test-assembler-a64.cc',
            'test-code-stubs.cc',
            'test-code-stubs-a64.cc',
            'test-disasm-a64.cc',
            'test-fuzz-a64.cc',
            'test-javascript-a64.cc',
            'test-js-a64-variables.cc'
          ],
        }],
        ['v8_target_arch=="mipsel"', {
          'sources': [
            'test-assembler-mips.cc',
            'test-code-stubs.cc',
            'test-code-stubs-mips.cc',
            'test-disasm-mips.cc',
            'test-macro-assembler-mips.cc'
          ],
        }],
        [ 'OS=="linux" or OS=="qnx"', {
          'sources': [
            'test-platform-linux.cc',
          ],
        }],
        [ 'OS=="mac"', {
          'sources': [
            'test-platform-macos.cc',
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
        ['component=="shared_library"', {
          # cctest can't be built against a shared library, so we need to
          # depend on the underlying static target in that case.
          'conditions': [
            ['v8_use_snapshot=="true"', {
              'dependencies': ['../../tools/gyp/v8.gyp:v8_snapshot'],
            },
            {
              'dependencies': [
                '../../tools/gyp/v8.gyp:v8_nosnapshot.<(v8_target_arch)',
              ],
            }],
          ],
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
            'off',  # compression
            '<@(file_list)',
          ],
        }
      ],
    },
  ],
}
