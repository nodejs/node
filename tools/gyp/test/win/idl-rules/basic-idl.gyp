# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'midl_out_dir': '<(SHARED_INTERMEDIATE_DIR)',
  },
  'target_defaults': {
    'configurations': {
      'Debug': {
        'msvs_configuration_platform': 'Win32',
      },
      'Debug_x64': {
        'inherit_from': ['Debug'],
        'msvs_configuration_platform': 'x64',
      },
    },
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
      'libraries': ['ole32.lib'],
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
    {
      'target_name': 'idl_explicit_action',
      'type': 'none',
      'sources': [
        'Window.idl',
      ],
      'actions': [{
        'action_name': 'blink_idl',
        'explicit_idl_action': 1,
        'msvs_cygwin_shell': 0,
        'inputs': [
          'Window.idl',
          'idl_compiler.py',
        ],
        'outputs': [
          'Window.cpp',
          'Window.h',
        ],
        'action': [
          'python',
          'idl_compiler.py',
          'Window.idl',
        ],
      }],
    },
  ],
}
