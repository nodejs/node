# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

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
        'link_settings': {
          'conditions': [
            ['OS=="win"', {
              'libraries': ['-ldbghelp.lib', '-lshlwapi.lib', '-lwinmm.lib', '-lws2_32.lib'],
            }],
            ['OS=="linux"', {
              'libraries': ['-ldl', '-lrt'],
            }],
          ],
        },
      },
      'actions': [
        {
          'action_name': 'build_with_gn_generate_build_files',
          # No need to list full set of inputs because after the initial
          # generation, ninja (run by the next action), will check to see
          # if build.ninja is stale.
          'inputs': [
            '../tools/node/build_gn.py',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/gn/build.ninja',
          ],
          'action': [
            'python',
            '../tools/node/build_gn.py',
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
            '--bundled-win-toolchain', '<(build_v8_with_gn_bundled_win_toolchain)',
            '--depot-tools', '<(build_v8_with_gn_depot_tools)',
          ],
          'conditions': [
            ['build_v8_with_gn_extra_gn_args != ""', {
              'action': [
                '--extra-gn-args', '<(build_v8_with_gn_extra_gn_args)',
              ],
            }],
            ['build_v8_with_gn_bundled_win_toolchain_root != ""', {
              'action': [
                '--bundled-win-toolchain-root', '<(build_v8_with_gn_bundled_win_toolchain_root)',
              ],
            }],
          ],
        },
        {
          'action_name': 'build_with_gn',
          'inputs': [
            '../tools/node/build_gn.py',
            '<(INTERMEDIATE_DIR)/gn/build.ninja',
          ],
          # Specify a non-existent output to make the target permanently dirty.
          # Alternatively, a depfile could be used, but then two dirty checks
          # would run: one by the outer build tool, and one by build_gn.py.
          'outputs': [
            '<(v8_base)',
            'does-not-exist',
          ],
          'action': [
            'python',
            '../tools/node/build_gn.py',
            '--build_path', '<(INTERMEDIATE_DIR)/gn',
            '--v8_path', '../',
            '--bundled-win-toolchain', '<(build_v8_with_gn_bundled_win_toolchain)',
            '--depot-tools', '<(build_v8_with_gn_depot_tools)',
            '--build',
          ],
          'conditions': [
            ['build_v8_with_gn_max_jobs!=""', {
              'action': [
                '--max-jobs', '<(build_v8_with_gn_max_jobs)',
              ],
            }],
            ['build_v8_with_gn_max_load!=""', {
              'action': [
                '--max-load', '<(build_v8_with_gn_max_load)',
              ],
            }],
          ],
          # Allows sub-ninja's build progress to be printed.
          'ninja_use_console': 1,
        },
      ],
    },
  ],
}
