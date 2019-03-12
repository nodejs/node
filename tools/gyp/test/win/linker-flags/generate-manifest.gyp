# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_generate_manifest_true',
      'type': 'executable',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCLinkerTool': {
          'EnableUAC': 'true',
          'GenerateManifest': 'true',
        },
        'VCManifestTool': {
          'EmbedManifest': 'false',
        },
      },
    },
    {
      'target_name': 'test_generate_manifest_false',
      'type': 'executable',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCLinkerTool': {
          'EnableUAC': 'true',
          'GenerateManifest': 'false',
        },
        'VCManifestTool': {
          'EmbedManifest': 'false',
        },
      },
    },
    {
      'target_name': 'test_generate_manifest_default',
      'type': 'executable',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCLinkerTool': {
          'EnableUAC': 'true',
        },
        'VCManifestTool': {
          'EmbedManifest': 'false',
        },
      },
    },
    {
      'target_name': 'test_generate_manifest_true_as_embedded',
      'type': 'executable',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCLinkerTool': {
          'EnableUAC': 'true',
          'GenerateManifest': 'true',
        },
        'VCManifestTool': {
          'EmbedManifest': 'true',
        },
      },
    },
    {
      'target_name': 'test_generate_manifest_false_as_embedded',
      'type': 'executable',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCLinkerTool': {
          'EnableUAC': 'true',
          'GenerateManifest': 'false',
        },
        'VCManifestTool': {
          'EmbedManifest': 'true',
        },
      },
    },
    {
      'target_name': 'test_generate_manifest_default_as_embedded',
      'type': 'executable',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCLinkerTool': {
          'EnableUAC': 'true',
        },
        'VCManifestTool': {
          'EmbedManifest': 'true',
        },
      },
    },
    {
      'target_name': 'test_generate_manifest_true_with_extra_manifest',
      'type': 'executable',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCLinkerTool': {
          'EnableUAC': 'true',
          'GenerateManifest': 'true',
        },
        'VCManifestTool': {
          'EmbedManifest': 'false',
          'AdditionalManifestFiles': 'extra.manifest;extra2.manifest',
        },
      },
    },
    {
      'target_name': 'test_generate_manifest_false_with_extra_manifest',
      'type': 'executable',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCLinkerTool': {
          'EnableUAC': 'true',
          'GenerateManifest': 'false',
        },
        'VCManifestTool': {
          'EmbedManifest': 'false',
          'AdditionalManifestFiles': 'extra.manifest;extra2.manifest',
        },
      },
    },
    {
      'target_name': 'test_generate_manifest_true_with_extra_manifest_list',
      'type': 'executable',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCLinkerTool': {
          'EnableUAC': 'true',
          'GenerateManifest': 'true',
        },
        'VCManifestTool': {
          'EmbedManifest': 'false',
          'AdditionalManifestFiles': [
            'extra.manifest',
            'extra2.manifest',
          ],
        },
      },
    },
    {
      'target_name': 'test_generate_manifest_false_with_extra_manifest_list',
      'type': 'executable',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCLinkerTool': {
          'EnableUAC': 'true',
          'GenerateManifest': 'false',
        },
        'VCManifestTool': {
          'EmbedManifest': 'false',
          'AdditionalManifestFiles': [
            'extra.manifest',
            'extra2.manifest',
          ],
        },
      },
    },
    {
      'target_name': 'test_generate_manifest_default_embed_default',
      'type': 'executable',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCLinkerTool': {
          'EnableUAC': 'true',
        },
      },
    },
  ]
}
