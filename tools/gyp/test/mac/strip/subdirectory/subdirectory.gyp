# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'nested_strip_save',
      'type': 'shared_library',
      'sources': [ 'nested_file.c', ],
      'xcode_settings': {
        'DEPLOYMENT_POSTPROCESSING': 'YES',
        'STRIP_INSTALLED_PRODUCT': 'YES',
        'STRIPFLAGS': '-s $(CHROMIUM_STRIP_SAVE_FILE)',
        'CHROMIUM_STRIP_SAVE_FILE': 'nested_strip.saves',
      },
    },
    {
      'target_name': 'nested_strip_save_postbuild',
      'type': 'shared_library',
      'sources': [ 'nested_file.c', ],
      'xcode_settings': {
        'DEPLOYMENT_POSTPROCESSING': 'YES',
        'STRIP_INSTALLED_PRODUCT': 'YES',
        'STRIPFLAGS': '-s $(CHROMIUM_STRIP_SAVE_FILE)',
        'CHROMIUM_STRIP_SAVE_FILE': 'nested_strip.saves',
      },
      'postbuilds': [
        {
          'postbuild_name': 'Action that reads CHROMIUM_STRIP_SAVE_FILE',
          'action': [
            './test_reading_save_file_from_postbuild.sh',
          ],
        },
      ],
    },
  ],
}

