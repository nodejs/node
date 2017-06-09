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
    'v8_enable_i18n_support%': 1,
    'v8_toolset_for_shell%': 'target',
  },
  'includes': ['../gypfiles/toolchain.gypi', '../gypfiles/features.gypi'],
  'target_defaults': {
    'type': 'executable',
    'dependencies': [
      '../src/v8.gyp:v8',
      '../src/v8.gyp:v8_libbase',
      '../src/v8.gyp:v8_libplatform',
    ],
    'include_dirs': [
      '..',
    ],
    'conditions': [
      ['v8_enable_i18n_support==1', {
        'dependencies': [
          '<(icu_gyp_path):icui18n',
          '<(icu_gyp_path):icuuc',
        ],
      }],
      ['OS=="win" and v8_enable_i18n_support==1', {
        'dependencies': [
          '<(icu_gyp_path):icudata',
        ],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'v8_shell',
      'sources': [
        'shell.cc',
      ],
      'conditions': [
        [ 'want_separate_host_toolset==1', {
          'toolsets': [ '<(v8_toolset_for_shell)', ],
        }],
      ],
    },
    {
      'target_name': 'hello-world',
      'sources': [
        'hello-world.cc',
      ],
    },
    {
      'target_name': 'process',
      'sources': [
        'process.cc',
      ],
    },
  ],
}
