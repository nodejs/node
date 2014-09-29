# Copyright 2013 the V8 project authors. All rights reserved.
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

# Compile time controlled V8 features.

{
  'variables': {
    'v8_compress_startup_data%': 'off',

    'v8_enable_disassembler%': 0,

    'v8_enable_gdbjit%': 0,

    'v8_object_print%': 0,

    'v8_enable_verify_heap%': 0,

    'v8_use_snapshot%': 'true',

    'v8_enable_verify_predictable%': 0,

    # With post mortem support enabled, metadata is embedded into libv8 that
    # describes various parameters of the VM for use by debuggers. See
    # tools/gen-postmortem-metadata.py for details.
    'v8_postmortem_support%': 'false',

    # Interpreted regexp engine exists as platform-independent alternative
    # based where the regular expression is compiled to a bytecode.
    'v8_interpreted_regexp%': 0,

    # Enable ECMAScript Internationalization API. Enabling this feature will
    # add a dependency on the ICU library.
    'v8_enable_i18n_support%': 1,

    # Enable compiler warnings when using V8_DEPRECATED apis.
    'v8_deprecation_warnings%': 0,

    # Use external files for startup data blobs:
    # the JS builtins sources and the start snapshot.
    'v8_use_external_startup_data%': 0,
  },
  'target_defaults': {
    'conditions': [
      ['v8_enable_disassembler==1', {
        'defines': ['ENABLE_DISASSEMBLER',],
      }],
      ['v8_enable_gdbjit==1', {
        'defines': ['ENABLE_GDB_JIT_INTERFACE',],
      }],
      ['v8_object_print==1', {
        'defines': ['OBJECT_PRINT',],
      }],
      ['v8_enable_verify_heap==1', {
        'defines': ['VERIFY_HEAP',],
      }],
      ['v8_enable_verify_predictable==1', {
        'defines': ['VERIFY_PREDICTABLE',],
      }],
      ['v8_interpreted_regexp==1', {
        'defines': ['V8_INTERPRETED_REGEXP',],
      }],
      ['v8_deprecation_warnings==1', {
        'defines': ['V8_DEPRECATION_WARNINGS',],
      }],
      ['v8_enable_i18n_support==1', {
        'defines': ['V8_I18N_SUPPORT',],
      }],
      ['v8_compress_startup_data=="bz2"', {
        'defines': ['COMPRESS_STARTUP_DATA_BZ2',],
      }],
      ['v8_use_external_startup_data==1', {
        'defines': ['V8_USE_EXTERNAL_STARTUP_DATA',],
      }],
    ],  # conditions
    'configurations': {
      'DebugBaseCommon': {
        'abstract': 1,
        'variables': {
          'v8_enable_extra_checks%': 1,
          'v8_enable_handle_zapping%': 1,
        },
        'conditions': [
          ['v8_enable_extra_checks==1', {
            'defines': ['ENABLE_EXTRA_CHECKS',],
          }],
          ['v8_enable_handle_zapping==1', {
            'defines': ['ENABLE_HANDLE_ZAPPING',],
          }],
        ],
      },  # Debug
      'Release': {
        'variables': {
          'v8_enable_extra_checks%': 0,
          'v8_enable_handle_zapping%': 1,
        },
        'conditions': [
          ['v8_enable_extra_checks==1', {
            'defines': ['ENABLE_EXTRA_CHECKS',],
          }],
          ['v8_enable_handle_zapping==1', {
            'defines': ['ENABLE_HANDLE_ZAPPING',],
          }],
        ],  # conditions
      },  # Release
    },  # configurations
  },  # target_defaults
}
