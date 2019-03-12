# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_mapfile_unset',
      'type': 'executable',
      'sources': ['mapfile.cc'],
    },
    {
      'target_name': 'test_mapfile_generate',
      'type': 'executable',
      'msvs_settings': {
        'VCLinkerTool': {
          'GenerateMapFile': 'true',
        },
      },
      'sources': ['mapfile.cc'],
    },
    {
      'target_name': 'test_mapfile_generate_exports',
      'type': 'executable',
      'msvs_settings': {
        'VCLinkerTool': {
          'GenerateMapFile': 'true',
          'MapExports': 'true',
        },
      },
      'sources': ['mapfile.cc'],
    },
    {
      'target_name': 'test_mapfile_generate_filename',
      'type': 'executable',
      'msvs_settings': {
        'VCLinkerTool': {
          'GenerateMapFile': 'true',
          'MapFileName': '<(PRODUCT_DIR)/custom_file_name.map',
        },
      },
      'sources': ['mapfile.cc'],
    },
  ]
}
