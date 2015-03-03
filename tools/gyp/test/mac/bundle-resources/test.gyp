# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'resource',
      'type': 'executable',
      'mac_bundle': 1,
      'mac_bundle_resources': [
        'secret.txt',
        'executable-file.sh',
      ],
    },
    # A rule with process_outputs_as_mac_bundle_resources should copy files
    # into the Resources folder.
    {
      'target_name': 'source_rule',
      'type': 'executable',
      'mac_bundle': 1,
      'sources': [
        'secret.txt',
      ],
      'rules': [
        {
          'rule_name': 'bundlerule',
          'extension': 'txt',
          'outputs': [
            '<(INTERMEDIATE_DIR)/<(RULE_INPUT_ROOT).txt',
          ],
          'action': ['./change.sh', '<(RULE_INPUT_PATH)', '<@(_outputs)'],
          'message': 'Running rule on <(RULE_INPUT_PATH)',
          'process_outputs_as_mac_bundle_resources': 1,
        },
      ],
    },
    # So should an ordinary rule acting on mac_bundle_resources.
    {
      'target_name': 'resource_rule',
      'type': 'executable',
      'mac_bundle': 1,
      'mac_bundle_resources': [
        'secret.txt',
      ],
      'rules': [
        {
          'rule_name': 'bundlerule',
          'extension': 'txt',
          'outputs': [
            '<(INTERMEDIATE_DIR)/<(RULE_INPUT_ROOT).txt',
          ],
          'action': ['./change.sh', '<(RULE_INPUT_PATH)', '<@(_outputs)'],
          'message': 'Running rule on <(RULE_INPUT_PATH)',
        },
      ],
    },
  ],
}

