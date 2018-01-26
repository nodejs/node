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
    'v8_random_seed%': 314159265,
    'v8_vector_stores%': 0,
    'embed_script%': "",
    'warmup_script%': "",
    'v8_extra_library_files%': [],
    'v8_experimental_extra_library_files%': [],
    'mksnapshot_exec': '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)mksnapshot<(EXECUTABLE_SUFFIX)',
    'v8_os_page_size%': 0,
    'v8_src': '../src',
    'v8_include': '../include',
    'v8_tools': '../tools',
  },
  'includes': ['../gypfiles/toolchain.gypi', '../gypfiles/features.gypi'],
  'targets': [
    {
      'target_name': 'v8_monolith',
      'type': 'none',
      'direct_dependent_settings': {
        'include_dirs': [
          '<(v8_include)',
        ],
      },
      'actions': [
        {
          'action_name': 'build_with_gn',
          'inputs': [
            '<(v8_tools)/node/build_gn.py',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/gn/obj/libv8_monolith.a',
            '<(INTERMEDIATE_DIR)/gn/args.gn',
          ],
          'action': [
            '<(v8_tools)/node/build_gn.py',
            '--mode', '<(CONFIGURATION_NAME)',
            '--v8_path', '../',
            '--build_path', '<(INTERMEDIATE_DIR)/gn',
            '--host_os', '<(host_os)',
            '--flag', 'v8_promise_internal_field_count=<(v8_promise_internal_field_count)',
            '--flag', 'target_cpu="<(target_arch)"',
            '--flag', 'target_os="<(OS)"',
            '--flag', 'v8_target_cpu="<(v8_target_arch)"',
            '--flag', 'v8_embedder_string="<(v8_embedder_string)"',
            '--flag', 'v8_use_snapshot=<(v8_use_snapshot)',
            '--flag', 'v8_optimized_debug=<(v8_optimized_debug)',
            '--flag', 'v8_enable_disassembler=<(v8_enable_disassembler)',
            '--flag', 'v8_postmortem_support=<(v8_postmortem_support)',
          ],
        },
      ],
    },
  ],
}
