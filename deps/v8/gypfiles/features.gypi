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
    'variables': {
      'v8_target_arch%': '<(target_arch)',
    },

    # Allows the embedder to add a custom suffix to the version string.
    'v8_embedder_string%': '',

    'v8_enable_disassembler%': 0,

    'v8_promise_internal_field_count%': 0,

    'v8_enable_gdbjit%': 0,

    'v8_enable_verify_csa%': 0,

    'v8_object_print%': 0,

    'v8_enable_verify_heap%': 0,

    'v8_trace_maps%': 0,

    # Enable the snapshot feature, for fast context creation.
    # http://v8project.blogspot.com/2015/09/custom-startup-snapshots.html
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

    # Enable compiler warnings when using V8_DEPRECATE_SOON apis.
    'v8_imminent_deprecation_warnings%': 0,

    # Set to 1 to enable DCHECKs in release builds.
    'dcheck_always_on%': 0,

    # Enable/disable JavaScript API accessors.
    'v8_js_accessors%': 0,

    # Temporary flag to allow embedders to update their microtasks scopes.
    'v8_check_microtasks_scopes_consistency%': 'false',

    # Enable concurrent marking.
    'v8_enable_concurrent_marking%': 1,

    # Controls the threshold for on-heap/off-heap Typed Arrays.
    'v8_typed_array_max_size_in_heap%': 64,
  },
  'target_defaults': {
    'conditions': [
      ['v8_embedder_string!=""', {
        'defines': ['V8_EMBEDDER_STRING="<(v8_embedder_string)"',],
      }],
      ['v8_enable_disassembler==1', {
        'defines': ['ENABLE_DISASSEMBLER',],
      }],
      ['v8_promise_internal_field_count!=0', {
        'defines': ['V8_PROMISE_INTERNAL_FIELD_COUNT','v8_promise_internal_field_count'],
      }],
      ['v8_enable_gdbjit==1', {
        'defines': ['ENABLE_GDB_JIT_INTERFACE',],
      }],
      ['v8_enable_verify_csa==1', {
        'defines': ['ENABLE_VERIFY_CSA',],
      }],
      ['v8_object_print==1', {
        'defines': ['OBJECT_PRINT',],
      }],
      ['v8_enable_verify_heap==1', {
        'defines': ['VERIFY_HEAP',],
      }],
      ['v8_trace_maps==1', {
        'defines': ['V8_TRACE_MAPS',],
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
      ['v8_imminent_deprecation_warnings==1', {
        'defines': ['V8_IMMINENT_DEPRECATION_WARNINGS',],
      }],
      ['v8_enable_i18n_support==1', {
        'defines': ['V8_INTL_SUPPORT',],
      }],
      ['v8_use_snapshot=="true" and v8_use_external_startup_data==1', {
        'defines': ['V8_USE_EXTERNAL_STARTUP_DATA',],
      }],
      ['dcheck_always_on!=0', {
        'defines': ['DEBUG',],
      }],
      ['v8_check_microtasks_scopes_consistency=="true"', {
        'defines': ['V8_CHECK_MICROTASKS_SCOPES_CONSISTENCY',],
      }],
      ['v8_enable_concurrent_marking==1', {
        'defines': ['V8_CONCURRENT_MARKING',],
      }],
    ],  # conditions
    'configurations': {
      'DebugBaseCommon': {
        'abstract': 1,
        'variables': {
          'v8_enable_handle_zapping%': 1,
        },
        'conditions': [
          ['v8_enable_handle_zapping==1', {
            'defines': ['ENABLE_HANDLE_ZAPPING',],
          }],
        ],
      },  # Debug
      'Release': {
        'variables': {
          'v8_enable_handle_zapping%': 0,
        },
        'conditions': [
          ['v8_enable_handle_zapping==1', {
            'defines': ['ENABLE_HANDLE_ZAPPING',],
          }],
        ],  # conditions
      },  # Release
    },  # configurations
    'defines': [
      'V8_GYP_BUILD',
      'V8_TYPED_ARRAY_MAX_SIZE_IN_HEAP=<(v8_typed_array_max_size_in_heap)',
    ],  # defines
  },  # target_defaults
}
