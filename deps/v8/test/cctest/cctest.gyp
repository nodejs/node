# Copyright 2010 the V8 project authors. All rights reserved.
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
  'target_defaults': {
    'conditions': [
      ['OS!="mac"', {
        # TODO(sgjesse): This is currently copied from v8.gyp, should probably
        # be refactored.
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
    ],
  },
  'targets': [
    {
      'target_name': 'cctest',
      'type': 'executable',
      'dependencies': [
        '../../tools/gyp/v8.gyp:v8',
      ],
      'include_dirs': [
        '../../src',
      ],
      'sources': [
        'cctest.cc',
        'gay-fixed.cc',
        'gay-precision.cc',
        'gay-shortest.cc',
        'test-accessors.cc',
        'test-alloc.cc',
        'test-api.cc',
        'test-ast.cc',
        'test-bignum.cc',
        'test-bignum-dtoa.cc',
        'test-circular-queue.cc',
        'test-compiler.cc',
        'test-conversions.cc',
        'test-cpu-profiler.cc',
        'test-dataflow.cc',
        'test-debug.cc',
        'test-decls.cc',
        'test-deoptimization.cc',
        'test-diy-fp.cc',
        'test-double.cc',
        'test-dtoa.cc',
        'test-fast-dtoa.cc',
        'test-fixed-dtoa.cc',
        'test-flags.cc',
        'test-func-name-inference.cc',
        'test-hashmap.cc',
        'test-heap.cc',
        'test-heap-profiler.cc',
        'test-list.cc',
        'test-liveedit.cc',
        'test-lock.cc',
        'test-log.cc',
        'test-log-utils.cc',
        'test-mark-compact.cc',
        'test-parsing.cc',
        'test-profile-generator.cc',
        'test-regexp.cc',
        'test-reloc-info.cc',
        'test-serialize.cc',
        'test-sockets.cc',
        'test-spaces.cc',
        'test-strings.cc',
        'test-strtod.cc',
        'test-thread-termination.cc',
        'test-threads.cc',
        'test-type-info.cc',
        'test-unbound-queue.cc',
        'test-utils.cc',
        'test-version.cc'
      ],
      'conditions': [
        ['v8_target_arch=="ia32"', {
          'sources': [
            'test-assembler-ia32.cc',
            'test-disasm-ia32.cc',
            'test-log-stack-tracer.cc'
          ],
        }],
        ['v8_target_arch=="x64"', {
          'sources': [
            'test-assembler-x64.cc',
            'test-macro-assembler-x64.cc',
            'test-log-stack-tracer.cc'
          ],
        }],
        ['v8_target_arch=="arm"', {
          'sources': [
            'test-assembler-arm.cc',
            'test-disasm-arm.cc'
          ],
        }],
        ['v8_target_arch=="mips"', {
          'sources': [
            'test-assembler-mips.cc',
            'test-mips.cc',
          ],
        }],
        [ 'OS=="linux"', {
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
        }],
      ],
    },
  ],
}
