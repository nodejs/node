# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'empty_bundle',
      'type': 'loadable_module',
      'mac_bundle': 1,
    },
    {
      'target_name': 'resource_bundle',
      'type': 'loadable_module',
      'mac_bundle': 1,
      'actions': [
        {
          'action_name': 'Add Resource',
          'inputs': [],
          'outputs': [
            '<(INTERMEDIATE_DIR)/app_manifest/foo.manifest',
          ],
          'action': [
            'touch', '<(INTERMEDIATE_DIR)/app_manifest/foo.manifest',
          ],
          'process_outputs_as_mac_bundle_resources': 1,
        },
      ],
    },
    {
      'target_name': 'dependent_on_resource_bundle',
      'type': 'executable',
      'sources': [ 'empty.c' ],
      'dependencies': [
        'resource_bundle',
      ],
    },

    {
      'target_name': 'alib',
      'type': 'static_library',
      'sources': [ 'fun.c' ]
    },
    { # No sources, but depends on a static_library so must be linked.
      'target_name': 'resource_framework',
      'type': 'shared_library',
      'mac_bundle': 1,
      'dependencies': [
        'alib',
      ],
      'actions': [
        {
          'action_name': 'Add Resource',
          'inputs': [],
          'outputs': [
            '<(INTERMEDIATE_DIR)/app_manifest/foo.manifest',
          ],
          'action': [
            'touch', '<(INTERMEDIATE_DIR)/app_manifest/foo.manifest',
          ],
          'process_outputs_as_mac_bundle_resources': 1,
        },
      ],
    },
    {
      'target_name': 'dependent_on_resource_framework',
      'type': 'executable',
      'sources': [ 'empty.c' ],
      'dependencies': [
        'resource_framework',
      ],
    },

    { # No actions, but still have resources.
      'target_name': 'mac_resource_bundle_no_actions',
      'product_extension': 'bundle',
      'type': 'executable',
      'mac_bundle': 1,
      'mac_bundle_resources': [
        'empty.txt',
      ],
    },
    {
      'target_name': 'bundle_dependent_on_resource_bundle_no_actions',
      'type': 'executable',
      'mac_bundle': 1,
      'sources': [ 'empty.c' ],
      'dependencies': [
        'mac_resource_bundle_no_actions',
      ],
      'mac_bundle_resources': [
        '<(PRODUCT_DIR)/mac_resource_bundle_no_actions.bundle',
      ],
    },
  ],
}

