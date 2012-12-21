# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'midl_out_dir': '<(SHARED_INTERMEDIATE_DIR)',
  },
  'targets': [
    {
      'target_name': 'idl_test',
      'type': 'executable',
      'sources': [
        'history_indexer.idl',
        '<(midl_out_dir)/history_indexer.h',
        '<(midl_out_dir)/history_indexer_i.c',
        'history_indexer_user.cc',
      ],
      'include_dirs': [
        '<(midl_out_dir)',
      ],
      'msvs_settings': {
        'VCMIDLTool': {
          'OutputDirectory': '<(midl_out_dir)',
          'HeaderFileName': '<(RULE_INPUT_ROOT).h',
         },
      },
    },
  ],
}
