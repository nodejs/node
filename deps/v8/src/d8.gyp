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
  'targets': [
    {
      'target_name': 'd8',
      'type': 'executable',
      'dependencies': [
        'd8_js2c#host',
        '../tools/gyp/v8.gyp:v8',
      ],
      'include_dirs+': [
        '../src',
      ],
      'defines': [
        'ENABLE_DEBUGGER_SUPPORT',
      ],
      'sources': [
        'd8.cc',
        'd8-debug.cc',
        '<(SHARED_INTERMEDIATE_DIR)/d8-js.cc',
      ],
      'conditions': [
        [ 'OS=="linux" or OS=="mac" or OS=="freebsd" or OS=="openbsd" or OS=="solaris"', {
          'sources': [ 'd8-posix.cc', ]
        }],
      ],
    },
    {
      'target_name': 'd8_js2c',
      'type': 'none',
      'toolsets': ['host'],
      'variables': {
        'js_files': [
          'd8.js',
        ],
      },
      'actions': [
        {
          'action_name': 'd8_js2c',
          'inputs': [
            '../tools/js2c.py',
            '<@(js_files)',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/d8-js.cc',
            '<(SHARED_INTERMEDIATE_DIR)/d8-js-empty.cc',
          ],
          'action': [
            'python',
            '../tools/js2c.py',
            '<@(_outputs)',
            'D8',
            '<@(js_files)'
          ],
        },
      ],
    }
  ],
}
