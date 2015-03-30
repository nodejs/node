# Copyright (c) 2013 Yandex LLC. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'test_manifest_exe',
      'type': 'executable',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCLinkerTool': {
          'LinkIncremental': '1',
        },
        'VCManifestTool': {
          'EmbedManifest': 'true',
        }
      },
    },
    {
      'target_name': 'test_manifest_dll',
      'type': 'loadable_module',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCLinkerTool': {
          'LinkIncremental': '1',
        },
        'VCManifestTool': {
          'EmbedManifest': 'true',
        }
      },
    },
    {
      'target_name': 'test_manifest_extra1',
      'type': 'executable',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCManifestTool': {
          'EmbedManifest': 'true',
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
          'EmbedManifest': 'true',
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
          'EmbedManifest': 'true',
          'AdditionalManifestFiles': [
            'extra.manifest',
            'extra2.manifest'
          ],
        }
      },
    },
    {
      'target_name': 'test_manifest_dll_inc',
      'type': 'loadable_module',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCLinkerTool': {
          'LinkIncremental': '2',
        },
        'VCManifestTool': {
          'EmbedManifest': 'true',
        }
      },
    },
    {
      'target_name': 'test_manifest_exe_inc',
      'type': 'executable',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCLinkerTool': {
          'LinkIncremental': '2',
        },
        'VCManifestTool': {
          'EmbedManifest': 'true',
        }
      },
    },
    {
      'target_name': 'test_manifest_exe_inc_no_embed',
      'type': 'executable',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCLinkerTool': {
          'LinkIncremental': '2',
        },
        'VCManifestTool': {
          'EmbedManifest': 'false',
        }
      },
    },
  ]
}
