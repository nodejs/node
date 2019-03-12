# Copyright 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # This command will be run from the directories of the .gyp files that
    # include this .gypi, the subdirectories dir_1 and dir_2, so use a
    # relative path from those directories to the script.
    'observed_value': '<!(python ../print_cwd_basename.py)',
  },
  'targets': [
    {
      'target_name': '<(target_name)',
      'type': 'none',
      'conditions': [
        ['observed_value != expected_value', {
          # Attempt to expand an undefined variable. This triggers a GYP
          # error.
          'assertion': '<(observed_value_must_equal_expected_value)',
        }],
      ],
    },
  ],
}
