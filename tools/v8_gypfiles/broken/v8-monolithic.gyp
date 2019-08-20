# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'v8_code': 1,
    'v8_random_seed%': 314159265,
    'v8_vector_stores%': 0,
    'v8_embed_script%': "",
    'v8_extra_library_files%': [],
    'mksnapshot_exec': '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)mksnapshot<(EXECUTABLE_SUFFIX)',
    'v8_os_page_size%': 0,
  },
  'includes': ['toolchain.gypi', 'features.gypi', 'inspector.gypi'],
  'targets': [
    {
      'target_name': 'v8_monolith',
      'type': 'none',
      'direct_dependent_settings': {
        'include_dirs': [
          '../include/',
        ],
      },
      'actions': [
        {
          'action_name': 'build_with_gn',
          'inputs': [
            '../tools//node/build_gn.py',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/gn/obj/libv8_monolith.a',
            '<(INTERMEDIATE_DIR)/gn/args.gn',
          ],
          'action': [
            '../tools//node/build_gn.py',
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
