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
    'v8_target_arch%': '<(target_arch)',

    'v8_current_cpu%': '<(target_arch)',

    # Emulate GN variables
    # https://chromium.googlesource.com/chromium/src/build/+/556c524beb09c332698debe1b47b065d5d029cd0/config/BUILDCONFIG.gn#269
    'conditions': [
      ['OS == "win" or OS == "winuwp"', {
        'is_win': 1,
      }, {
        'is_win': 0,
      }],
      ['OS == "fuchsia"', {
        'is_fuchsia': 1,
      }, {
        'is_fuchsia': 0,
      }],
      ['OS=="android"', { # GYP reverts OS to linux so use `-D OS=android`
        'is_android': 1,
      }, {
        'is_android': 0,
      }],
      # flattened (!is_win && !is_fuchsia) because of GYP evaluation order
      ['not (OS == "win" or OS == "winuwp") and not (OS == "fuchsia")', {
        'is_posix': 1,
      }, {
        'is_posix': 0,
      }],
      ['component and "library" in component', {
        'is_component_build': 1,
      }, {
        'is_component_build': 0,
      }],
    ],
    'is_debug%': 0,

    # Variables from BUILD.gn

    # Set to 1 to enable DCHECKs in release builds.
    'dcheck_always_on%': 0,

    # Sets -DV8_ENABLE_FUTURE.
    'v8_enable_future%': 0,

    # Sets -DSYSTEM_INSTRUMENTATION. Enables OS-dependent event tracing
    'v8_enable_system_instrumentation%': 1,

    # Sets -DVERIFY_HEAP.
    'v8_enable_verify_heap%': 0,

    # Sets -DVERIFY_PREDICTABLE
    'v8_enable_verify_predictable%': 0,

    # Enable compiler warnings when using V8_DEPRECATED apis.
    'v8_deprecation_warnings%': 0,

    # Enable compiler warnings when using V8_DEPRECATE_SOON apis.
    'v8_imminent_deprecation_warnings%': 0,

    # Allows the embedder to add a custom suffix to the version string.
    'v8_embedder_string%': '',

    # Sets -dENABLE_DISASSEMBLER.
    'v8_enable_disassembler%': 0,

    # Sets the number of internal fields on promise objects.
    'v8_promise_internal_field_count%': 0,

    # Sets -dENABLE_GDB_JIT_INTERFACE.
    'v8_enable_gdbjit%': 0,

    # Currently set for node by common.gypi, avoiding default because of gyp file bug.
    # Should be turned on only for debugging.
    #'v8_enable_handle_zapping%': 0,

    # Enable fast mksnapshot runs.
    'v8_enable_fast_mksnapshot%': 0,

    # Enable the registration of unwinding info for Windows/x64 and ARM64.
    'v8_win64_unwinding_info%': 1,

    # Enable code comments for builtins in the snapshot (impacts performance).
    'v8_enable_snapshot_code_comments%': 0,

    # Enable native counters from the snapshot (impacts performance, sets
    # -dV8_SNAPSHOT_NATIVE_CODE_COUNTERS).
    # This option will generate extra code in the snapshot to increment counters,
    # as per the --native-code-counters flag.
    'v8_enable_snapshot_native_code_counters%': 0,

    # Enable code-generation-time checking of types in the CodeStubAssembler.
    'v8_enable_verify_csa%': 0,

    # Enable pointer compression (sets -dV8_COMPRESS_POINTERS).
    'v8_enable_pointer_compression%': 0,
    'v8_enable_31bit_smis_on_64bit_arch%': 0,

    # Reverse JS arguments order in the stack (sets -dV8_REVERSE_JSARGS).
    'v8_enable_reverse_jsargs%': 0,

    # Sets -dOBJECT_PRINT.
    'v8_enable_object_print%': 0,

    # Sets -dV8_TRACE_MAPS.
    'v8_enable_trace_maps%': 0,

    # Sets -dV8_ENABLE_CHECKS.
    'v8_enable_v8_checks%': 0,

    # Sets -dV8_TRACE_IGNITION.
    'v8_enable_trace_ignition%': 0,

    # Sets -dV8_TRACE_FEEDBACK_UPDATES.
    'v8_enable_trace_feedback_updates%': 0,

    # Enables various testing features.
    'v8_enable_test_features%': 0,

    # Enables raw heap snapshots containing internals. Used for debugging memory
    # on platform and embedder level.
    'v8_enable_raw_heap_snapshots%': 0,

    # With post mortem support enabled, metadata is embedded into libv8 that
    # describes various parameters of the VM for use by debuggers. See
    # tools/gen-postmortem-metadata.py for details.
    'v8_postmortem_support%': 0,

    # Use Siphash as added protection against hash flooding attacks.
    'v8_use_siphash%': 0,

    # Use Perfetto (https://perfetto.dev) as the default TracingController. Not
    # currently implemented.
    'v8_use_perfetto%': 0,

    # Controls the threshold for on-heap/off-heap Typed Arrays.
    'v8_typed_array_max_size_in_heap%': 64,

    # Temporary flag to allow embedders to update their microtasks scopes
    # while rolling in a new version of V8.
    'v8_check_microtasks_scopes_consistency%': 0,

    # Enable mitigations for executing untrusted code.
    'v8_untrusted_code_mitigations%': 1,

    # Enable minor mark compact.
    'v8_enable_minor_mc%': 1,

    # Enable lazy source positions by default.
    'v8_enable_lazy_source_positions%': 1,

    # Enable third party HEAP library
    'v8_enable_third_party_heap%': 0,

    # Libaries used by third party heap
    'v8_third_party_heap_libs%': [],

    # Source code used by third party heap
    'v8_third_party_heap_files%': [],

    # Disable write barriers when GCs are non-incremental and
    # heap has single generation.
    'v8_disable_write_barriers%': 0,

    # Redirect allocation in young generation so that there will be
    # only one single generation.
    'v8_enable_single_generation%': 0,

    # Use token threaded dispatch for the regular expression interpreter.
    # Use switch-based dispatch if this is false.
    'v8_enable_regexp_interpreter_threaded_dispatch%': 1,

    # Disable all snapshot compression.
    'v8_enable_snapshot_compression%': 1,

    # Enable control-flow integrity features, such as pointer authentication
    # for ARM64.
    'v8_control_flow_integrity%': 0,

    # Enable V8 zone compression experimental feature.
    # Sets -DV8_COMPRESS_ZONES.
    'v8_enable_zone_compression%': 0,

    # Experimental feature for collecting per-class zone memory stats.
    # Requires use_rtti = true
    'v8_enable_precise_zone_stats%': 0,

    # Experimental feature for tracking constness of properties in non-global
    # dictionaries. Enabling this also always keeps prototypes in dict mode,
    # meaning that they are not switched to fast mode.
    # Sets -DV8_DICT_PROPERTY_CONST_TRACKING
    'v8_dict_property_const_tracking%': 0,

    # Variables from v8.gni

    # Enable ECMAScript Internationalization API. Enabling this feature will
    # add a dependency on the ICU library.
    'v8_enable_i18n_support%': 1,

    # Lite mode disables a number of performance optimizations to reduce memory
    # at the cost of performance.
    # Sets --DV8_LITE_MODE.
    'v8_enable_lite_mode%': 0,

    # Include support for WebAssembly. If disabled, the 'WebAssembly' global
    # will not be available, and embedder APIs to generate WebAssembly modules
    # will fail.
    'v8_enable_webassembly%': 1,
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
        'defines': ['V8_PROMISE_INTERNAL_FIELD_COUNT=<(v8_promise_internal_field_count)'],
      }],
      ['v8_enable_raw_heap_snapshots==1', {
        'defines': ['V8_ENABLE_RAW_HEAP_SNAPSHOTS',],
      }],
      ['v8_enable_future==1', {
        'defines': ['V8_ENABLE_FUTURE',],
      }],
      ['v8_enable_lite_mode==1', {
        'defines': ['V8_LITE_MODE',],
      }],
      ['v8_enable_gdbjit==1', {
        'defines': ['ENABLE_GDB_JIT_INTERFACE',],
      }],
      ['v8_enable_minor_mc==1', {
        'defines': ['ENABLE_MINOR_MC',],
      }],
      ['v8_enable_pointer_compression==1', {
        'defines': ['V8_COMPRESS_POINTERS',],
      }],
      ['v8_enable_pointer_compression==1 or v8_enable_31bit_smis_on_64bit_arch==1', {
        'defines': ['V8_31BIT_SMIS_ON_64BIT_ARCH',],
      }],
      ['v8_enable_zone_compression==1', {
        'defines': ['V8_COMPRESS_ZONES',],
      }],
      ['v8_enable_object_print==1', {
        'defines': ['OBJECT_PRINT',],
      }],
      ['v8_enable_verify_heap==1', {
        'defines': ['VERIFY_HEAP',],
      }],
      ['v8_enable_verify_predictable==1', {
        'defines': ['VERIFY_PREDICTABLE',],
      }],
      ['v8_enable_trace_maps==1', {
        'defines': ['V8_TRACE_MAPS',],
      }],
      ['v8_enable_trace_ignition==1', {
        'defines': ['V8_TRACE_IGNITION',],
      }],
      ['v8_enable_trace_feedback_updates==1', {
        'defines': ['V8_TRACE_FEEDBACK_UPDATES',],
      }],
      ['v8_enable_test_features==1', {
        'defines': [
          'V8_ENABLE_ALLOCATION_TIMEOUT',
          'V8_ENABLE_FORCE_SLOW_PATH',
          'V8_ENABLE_DOUBLE_CONST_STORE_CHECK',
        ],
      }],
      ['v8_enable_v8_checks==1', {
        'defines': ['V8_ENABLE_CHECKS',],
      }],
      ['v8_deprecation_warnings==1', {
        'defines': ['V8_DEPRECATION_WARNINGS',],
      },{
        'defines!': ['V8_DEPRECATION_WARNINGS',],
      }],
      ['v8_imminent_deprecation_warnings==1', {
        'defines': ['V8_IMMINENT_DEPRECATION_WARNINGS',],
      },{
        'defines!': ['V8_IMMINENT_DEPRECATION_WARNINGS',],
      }],
      ['v8_enable_reverse_jsargs==1', {
        'defines': ['V8_REVERSE_JSARGS',],
      }],
      ['v8_enable_i18n_support==1', {
        'defines': ['V8_INTL_SUPPORT',],
      }],
      # Refs: https://github.com/nodejs/node/pull/23801
      # ['v8_enable_handle_zapping==1', {
      #  'defines': ['ENABLE_HANDLE_ZAPPING',],
      # }],
      ['v8_enable_snapshot_native_code_counters==1', {
        'defines': ['V8_SNAPSHOT_NATIVE_CODE_COUNTERS',],
      }],
      ['v8_enable_single_generation==1', {
        'defines': ['V8_ENABLE_SINGLE_GENERATION',],
      }],
      ['v8_disable_write_barriers==1', {
        'defines': ['V8_DISABLE_WRITE_BARRIERS',],
      }],
      ['v8_enable_third_party_heap==1', {
        'defines': ['V8_ENABLE_THIRD_PARTY_HEAP',],
      }],
      ['v8_enable_lazy_source_positions==1', {
        'defines': ['V8_ENABLE_LAZY_SOURCE_POSITIONS',],
      }],
      ['v8_check_microtasks_scopes_consistency==1', {
        'defines': ['V8_CHECK_MICROTASKS_SCOPES_CONSISTENCY',],
      }],
      ['v8_use_siphash==1', {
        'defines': ['V8_USE_SIPHASH',],
      }],
      ['dcheck_always_on!=0', {
        'defines': ['DEBUG',],
      }],
      ['v8_enable_verify_csa==1', {
        'defines': ['ENABLE_VERIFY_CSA',],
      }],
      ['v8_untrusted_code_mitigations==0', {
        'defines': ['DISABLE_UNTRUSTED_CODE_MITIGATIONS',],
      }],
      ['v8_use_perfetto==1', {
        'defines': ['V8_USE_PERFETTO',],
      }],
      ['v8_win64_unwinding_info==1', {
        'defines': ['V8_WIN64_UNWINDING_INFO',],
      }],
      ['v8_enable_regexp_interpreter_threaded_dispatch==1', {
        'defines': ['V8_ENABLE_REGEXP_INTERPRETER_THREADED_DISPATCH',],
      }],
      ['v8_enable_snapshot_compression==1', {
        'defines': ['V8_SNAPSHOT_COMPRESSION',],
      }],
      ['v8_control_flow_integrity==1', {
        'defines': ['V8_ENABLE_CONTROL_FLOW_INTEGRITY',],
      }],
      ['v8_enable_precise_zone_stats==1', {
        'defines': ['V8_ENABLE_PRECISE_ZONE_STATS',],
      }],
      ['v8_enable_system_instrumentation==1', {
        'defines': ['V8_ENABLE_SYSTEM_INSTRUMENTATION',],
      }],
      ['v8_enable_webassembly==1', {
        'defines': ['V8_ENABLE_WEBASSEMBLY',],
      }],
      ['v8_dict_property_const_tracking==1', {
        'defines': ['V8_DICT_PROPERTY_CONST_TRACKING',],
      }],
    ],  # conditions
    'defines': [
      'V8_GYP_BUILD',
      'V8_TYPED_ARRAY_MAX_SIZE_IN_HEAP=<(v8_typed_array_max_size_in_heap)',
    ],  # defines
  },  # target_defaults
}
