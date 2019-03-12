# Copyright 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'enable_uac',
      'type': 'executable',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCManifestTool': {
          'EmbedManifest': 'true',
        }
      },
    },
    {
      'target_name': 'enable_uac_no',
      'type': 'executable',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCLinkerTool': {
          'EnableUAC': 'false',
        },
        'VCManifestTool': {
          'EmbedManifest': 'true',
        }
      },
    },
    {
      'target_name': 'enable_uac_admin',
      'type': 'executable',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCLinkerTool': {
          'UACExecutionLevel': 2,
          'UACUIAccess': 'true',
        },
        'VCManifestTool': {
          'EmbedManifest': 'true',
        }
      },
    },
  ]
}
