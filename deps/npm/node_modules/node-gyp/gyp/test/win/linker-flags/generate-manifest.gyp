# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_manifest_exe',
      'type': 'executable',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCManifestTool': {
          'EmbedManifest': 'false',
        }
      },
    },
    {
      'target_name': 'test_manifest_dll',
      'type': 'shared_library',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCManifestTool': {
          'EmbedManifest': 'false',
        }
      },
    },
    {
      'target_name': 'test_manifest_extra1',
      'type': 'executable',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCManifestTool': {
          'EmbedManifest': 'false',
          'AdditionalManifestFiles': 'extra.manifest',
        }
      },
    },
    {
      'target_name': 'test_manifest_extra2',
      'type': 'executable',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCManifestTool': {
          'EmbedManifest': 'false',
          'AdditionalManifestFiles': 'extra.manifest;extra2.manifest',
        }
      },
    },
    {
      'target_name': 'test_manifest_extra_list',
      'type': 'executable',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCManifestTool': {
          'EmbedManifest': 'false',
          'AdditionalManifestFiles': [
            'extra.manifest',
            'extra2.manifest'
          ],
        }
      },
    },
  ]
}
