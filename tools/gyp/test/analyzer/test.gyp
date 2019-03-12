# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# These gyp files create the following dependencies:
#
# test.gyp:
#   #exe -> subdir/subdir.gyp#foo, subdir/subdir2/subdir2.gyp#subdir2
#     foo.c
#     subdir/subdir_source2.c
#     conditional_source.c (if test_variable==1)
#     action_input.c
#     action_output.c
#     rule_input.c
#     rule_output.pdf
#   #exe2
#     exe2.c
#   #exe3 -> subdir/subdir.gyp#foo, subdir/subdir.gyp#subdir2a
#     exe3.c
#   #allx (type none) -> exe, exe3
# 
# subdir/subdir.gyp
#   #foo
#     subdir/subdir_source.c
#     parent_source.c
#   #subdir2a -> subdir2b
#     subdir/subdir2_source.c
#   #subdir2b
#     subdir/subdir2b_source.c
# 
# subdir/subdir2/subdir2.gyp
#   #subdir2
#     subdir/subdir_source.h

{
  'variables': {
    'test_variable%': 0,
    'variable_path': 'subdir',
   },
  'targets': [
    {
      'target_name': 'exe',
      'type': 'executable',
      'dependencies': [
        'subdir/subdir.gyp:foo',
        'subdir/subdir2/subdir2.gyp:subdir2',
      ],
      'sources': [
        'foo.c',
        '<(variable_path)/subdir_source2.c',
      ],
      'conditions': [
        ['test_variable==1', {
          'sources': [
            'conditional_source.c',
          ],
        }],
      ],
      'actions': [
        {
          'action_name': 'action',
          'inputs': [
            '<(PRODUCT_DIR)/product_dir_input.c',
            'action_input.c',
            '../bad_path1.h',
            '../../bad_path2.h',
            './rel_path1.h',
          ],
          'outputs': [
            'action_output.c',
          ],
        },
      ],
      'rules': [
        {
          'rule_name': 'rule',
          'extension': 'pdf',
          'inputs': [
            'rule_input.c',
          ],
          'outputs': [
            'rule_output.pdf',
          ],
        },
      ],
    },
    {
      'target_name': 'exe2',
      'type': 'executable',
      'sources': [
        'exe2.c',
      ],
    },
    {
      'target_name': 'exe3',
      'type': 'executable',
      'dependencies': [
        'subdir/subdir.gyp:foo',
        'subdir/subdir.gyp:subdir2a',
      ],
      'sources': [
        'exe3.c',
      ],
    },
    {
      'target_name': 'allx',
      'type': 'none',
      'dependencies': [
        'exe',
        'exe3',
      ],
    },
  ],
}
