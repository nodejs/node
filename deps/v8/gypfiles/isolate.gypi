# Copyright 2015 the V8 project authors. All rights reserved.
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is meant to be included into a target to provide a rule
# to "build" .isolate files into a .isolated file.
#
# To use this, create a gyp target with the following form:
# 'conditions': [
#   ['test_isolation_mode != "noop"', {
#     'targets': [
#       {
#         'target_name': 'foo_test_run',
#         'type': 'none',
#         'dependencies': [
#           'foo_test',
#         ],
#         'includes': [
#           '../gypfiles/isolate.gypi',
#         ],
#         'sources': [
#           'foo_test.isolate',
#         ],
#       },
#     ],
#   }],
# ],
#
# Note: foo_test.isolate is included and a source file. It is an inherent
# property of the .isolate format. This permits to define GYP variables but is
# a stricter format than GYP so isolate.py can read it.
#
# The generated .isolated file will be:
#   <(PRODUCT_DIR)/foo_test.isolated
#
# See http://dev.chromium.org/developers/testing/isolated-testing/for-swes
# for more information.

{
  'rules': [
    {
      'rule_name': 'isolate',
      'extension': 'isolate',
      'inputs': [
        # Files that are known to be involved in this step.
        '<(DEPTH)/tools/isolate_driver.py',
        '<(DEPTH)/tools/swarming_client/isolate.py',
        '<(DEPTH)/tools/swarming_client/run_isolated.py',
      ],
      'outputs': [
        '<(PRODUCT_DIR)/<(RULE_INPUT_ROOT).isolated',
      ],
      'action': [
        'python',
        '<(DEPTH)/tools/isolate_driver.py',
        '<(test_isolation_mode)',
        '--isolated', '<(PRODUCT_DIR)/<(RULE_INPUT_ROOT).isolated',
        '--isolate', '<(RULE_INPUT_PATH)',

        # Variables should use the -V FOO=<(FOO) form so frequent values,
        # like '0' or '1', aren't stripped out by GYP. Run 'isolate.py help'
        # for more details.

        # Path variables are used to replace file paths when loading a .isolate
        # file
        '--path-variable', 'DEPTH', '<(DEPTH)',
        '--path-variable', 'PRODUCT_DIR', '<(PRODUCT_DIR)',

        '--config-variable', 'CONFIGURATION_NAME=<(CONFIGURATION_NAME)',
        '--config-variable', 'OS=<(OS)',
        '--config-variable', 'asan=<(asan)',
        '--config-variable', 'cfi_vptr=<(cfi_vptr)',
        '--config-variable', 'gcmole=<(gcmole)',
        '--config-variable', 'has_valgrind=<(has_valgrind)',
        '--config-variable', 'icu_use_data_file_flag=<(icu_use_data_file_flag)',
        '--config-variable', 'msan=<(msan)',
        '--config-variable', 'tsan=<(tsan)',
        '--config-variable', 'coverage=<(coverage)',
        '--config-variable', 'sanitizer_coverage=<(sanitizer_coverage)',
        '--config-variable', 'component=<(component)',
        '--config-variable', 'target_arch=<(target_arch)',
        '--config-variable', 'v8_use_external_startup_data=<(v8_use_external_startup_data)',
        '--config-variable', 'v8_use_snapshot=<(v8_use_snapshot)',
      ],
      'conditions': [
        ['OS=="win"', {
          'action': [
            '--config-variable', 'msvs_version=2013',
          ],
        }, {
          'action': [
            '--config-variable', 'msvs_version=0',
          ],
        }],
      ],
    },
  ],
}
